/* filter.c: filter framework functions
 *
 * Copyright (C) 2006-2014 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#include "cgit.h"
#include "html.h"
#include "filter-internal.h"

static inline void reap_filter(struct cgit_filter *filter)
{
	if (filter && filter->cleanup)
		filter->cleanup(filter);
}

void cgit_cleanup_filters(void)
{
	int i;
	reap_filter(ctx.cfg.about_filter);
	reap_filter(ctx.cfg.commit_filter);
	reap_filter(ctx.cfg.source_filter);
	reap_filter(ctx.cfg.email_filter);
	reap_filter(ctx.cfg.owner_filter);
	reap_filter(ctx.cfg.auth_filter);
	for (i = 0; i < cgit_repolist.count; ++i) {
		reap_filter(cgit_repolist.repos[i].about_filter);
		reap_filter(cgit_repolist.repos[i].commit_filter);
		reap_filter(cgit_repolist.repos[i].source_filter);
		reap_filter(cgit_repolist.repos[i].email_filter);
		reap_filter(cgit_repolist.repos[i].owner_filter);
	}
}

#ifdef NO_LUA
void cgit_init_filters(void)
{
}
#endif


int cgit_open_filter(struct cgit_filter *filter, ...)
{
	int result;
	va_list ap;
	if (!filter)
		return 0;
	va_start(ap, filter);
	result = filter->open(filter, ap);
	va_end(ap);
	return result;
}

int cgit_close_filter(struct cgit_filter *filter)
{
	if (!filter)
		return 0;
	return filter->close(filter);
}

void cgit_fprintf_filter(struct cgit_filter *filter, FILE *f, const char *prefix)
{
	filter->fprintfp(filter, f, prefix);
}



static const struct {
	const char *prefix;
	struct cgit_filter *(*ctor)(const char *cmd, int argument_count);
} filter_specs[] = {
	{ "exec", cgit_new_exec_filter },
#ifndef NO_LUA
	{ "lua", cgit_new_lua_filter },
#endif
};

struct cgit_filter *cgit_new_filter(const char *cmd, filter_type filtertype)
{
	char *colon;
	int i;
	size_t len;
	int argument_count;

	if (!cmd || !cmd[0])
		return NULL;

	colon = strchr(cmd, ':');
	len = colon - cmd;
	/*
	 * In case we're running on Windows, don't allow a single letter before
	 * the colon.
	 */
	if (len == 1)
		colon = NULL;

	switch (filtertype) {
		case AUTH:
			argument_count = 12;
			break;

		case EMAIL:
			argument_count = 2;
			break;

		case OWNER:
			argument_count = 0;
			break;

		case SOURCE:
		case ABOUT:
			argument_count = 1;
			break;

		case COMMIT:
		default:
			argument_count = 0;
			break;
	}

	/* If no prefix is given, exec filter is the default. */
	if (!colon)
		return cgit_new_exec_filter(cmd, argument_count);

	for (i = 0; i < ARRAY_SIZE(filter_specs); i++) {
		if (len == strlen(filter_specs[i].prefix) &&
		    !strncmp(filter_specs[i].prefix, cmd, len))
			return filter_specs[i].ctor(colon + 1, argument_count);
	}

	die("Invalid filter type: %.*s", (int) len, cmd);
}
