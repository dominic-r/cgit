#include "cgit.h"
#include "html.h"
#include "filter-internal.h"
#ifndef NO_LUA
#include <dlfcn.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#ifndef NO_LUA
static ssize_t (*libc_write)(int fd, const void *buf, size_t count);
static ssize_t (*filter_write)(struct cgit_filter *base, const void *buf, size_t count) = NULL;
static struct cgit_filter *current_write_filter = NULL;

void cgit_init_filters(void)
{
	libc_write = dlsym(RTLD_NEXT, "write");
	if (!libc_write)
		die("Could not locate libc's write function");
}

ssize_t write(int fd, const void *buf, size_t count)
{
	if (fd != STDOUT_FILENO || !filter_write)
		return libc_write(fd, buf, count);
	return filter_write(current_write_filter, buf, count);
}

static inline void hook_write(struct cgit_filter *filter, ssize_t (*new_write)(struct cgit_filter *base, const void *buf, size_t count))
{
	/* We want to avoid buggy nested patterns. */
	assert(filter_write == NULL);
	assert(current_write_filter == NULL);
	current_write_filter = filter;
	filter_write = new_write;
}

static inline void unhook_write(void)
{
	assert(filter_write != NULL);
	assert(current_write_filter != NULL);
	filter_write = NULL;
	current_write_filter = NULL;
}

struct lua_filter {
	struct cgit_filter base;
	char *script_file;
	lua_State *lua_state;
};

static void error_lua_filter(struct lua_filter *filter)
{
	die("Lua error in %s: %s", filter->script_file, lua_tostring(filter->lua_state, -1));
	lua_pop(filter->lua_state, 1);
}

static ssize_t write_lua_filter(struct cgit_filter *base, const void *buf, size_t count)
{
	struct lua_filter *filter = (struct lua_filter *)base;

	lua_getglobal(filter->lua_state, "filter_write");
	lua_pushlstring(filter->lua_state, buf, count);
	if (lua_pcall(filter->lua_state, 1, 0, 0)) {
		error_lua_filter(filter);
		errno = EIO;
		return -1;
	}
	return count;
}

static inline int hook_lua_filter(lua_State *lua_state, void (*fn)(const char *txt))
{
	const char *str;
	ssize_t (*save_filter_write)(struct cgit_filter *base, const void *buf, size_t count);
	struct cgit_filter *save_filter;

	str = lua_tostring(lua_state, 1);
	if (!str)
		return 0;

	save_filter_write = filter_write;
	save_filter = current_write_filter;
	unhook_write();
	fn(str);
	hook_write(save_filter, save_filter_write);

	return 0;
}

static int html_lua_filter(lua_State *lua_state)
{
	return hook_lua_filter(lua_state, html);
}

static int html_txt_lua_filter(lua_State *lua_state)
{
	return hook_lua_filter(lua_state, html_txt);
}

static int html_attr_lua_filter(lua_State *lua_state)
{
	return hook_lua_filter(lua_state, html_attr);
}

static int html_url_path_lua_filter(lua_State *lua_state)
{
	return hook_lua_filter(lua_state, html_url_path);
}

static int html_url_arg_lua_filter(lua_State *lua_state)
{
	return hook_lua_filter(lua_state, html_url_arg);
}

static int html_include_lua_filter(lua_State *lua_state)
{
	return hook_lua_filter(lua_state, (void (*)(const char *))html_include);
}

static void cleanup_lua_filter(struct cgit_filter *base)
{
	struct lua_filter *filter = (struct lua_filter *)base;

	if (!filter->lua_state)
		return;

	lua_close(filter->lua_state);
	filter->lua_state = NULL;
	if (filter->script_file) {
		free(filter->script_file);
		filter->script_file = NULL;
	}
}

static int init_lua_filter(struct lua_filter *filter)
{
	if (filter->lua_state)
		return 0;

	if (!(filter->lua_state = luaL_newstate()))
		return 1;

	luaL_openlibs(filter->lua_state);

	lua_pushcfunction(filter->lua_state, html_lua_filter);
	lua_setglobal(filter->lua_state, "html");
	lua_pushcfunction(filter->lua_state, html_txt_lua_filter);
	lua_setglobal(filter->lua_state, "html_txt");
	lua_pushcfunction(filter->lua_state, html_attr_lua_filter);
	lua_setglobal(filter->lua_state, "html_attr");
	lua_pushcfunction(filter->lua_state, html_url_path_lua_filter);
	lua_setglobal(filter->lua_state, "html_url_path");
	lua_pushcfunction(filter->lua_state, html_url_arg_lua_filter);
	lua_setglobal(filter->lua_state, "html_url_arg");
	lua_pushcfunction(filter->lua_state, html_include_lua_filter);
	lua_setglobal(filter->lua_state, "html_include");

	if (luaL_dofile(filter->lua_state, filter->script_file)) {
		error_lua_filter(filter);
		lua_close(filter->lua_state);
		filter->lua_state = NULL;
		return 1;
	}
	return 0;
}

static int open_lua_filter(struct cgit_filter *base, va_list ap)
{
	struct lua_filter *filter = (struct lua_filter *)base;
	int i;

	if (init_lua_filter(filter))
		return 1;

	hook_write(base, write_lua_filter);

	lua_getglobal(filter->lua_state, "filter_open");
	for (i = 0; i < filter->base.argument_count; ++i)
		lua_pushstring(filter->lua_state, va_arg(ap, char *));
	if (lua_pcall(filter->lua_state, filter->base.argument_count, 0, 0)) {
		error_lua_filter(filter);
		return 1;
	}
	return 0;
}

static int close_lua_filter(struct cgit_filter *base)
{
	struct lua_filter *filter = (struct lua_filter *)base;
	int ret = 0;

	lua_getglobal(filter->lua_state, "filter_close");
	if (lua_pcall(filter->lua_state, 0, 1, 0)) {
		error_lua_filter(filter);
		ret = -1;
	} else {
		ret = lua_tonumber(filter->lua_state, -1);
		lua_pop(filter->lua_state, 1);
	}

	unhook_write();
	return ret;
}

static void fprintf_lua_filter(struct cgit_filter *base, FILE *f, const char *prefix)
{
	struct lua_filter *filter = (struct lua_filter *)base;
	fprintf(f, "%slua:%s\n", prefix, filter->script_file);
}


struct cgit_filter *cgit_new_lua_filter(const char *cmd, int argument_count)
{
	struct lua_filter *filter;

	filter = xmalloc(sizeof(*filter));
	memset(filter, 0, sizeof(*filter));
	filter->base.open = open_lua_filter;
	filter->base.close = close_lua_filter;
	filter->base.fprintfp = fprintf_lua_filter;
	filter->base.cleanup = cleanup_lua_filter;
	filter->base.argument_count = argument_count;
	filter->script_file = xstrdup(cmd);

	return &filter->base;
}

#endif
