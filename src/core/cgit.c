/* cgit.c: cgi for the git scm
 *
 * Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "cache.h"
#include "cmd.h"
#include "html.h"
#include "ui-shared.h"
#include "ui-stats.h"
#include "ui-blob.h"
#include "ui-summary.h"
#include "cgit-main.h"

const char *cgit_version = CGIT_VERSION;

__attribute__((constructor))
static void constructor_environment()
{
	/* Do not look in /etc/ for gitconfig and gitattributes. */
	setenv("GIT_CONFIG_NOSYSTEM", "1", 1);
	setenv("GIT_ATTR_NOSYSTEM", "1", 1);
	unsetenv("HOME");
	unsetenv("XDG_CONFIG_HOME");
}

static void process_request(void)
{
	struct cgit_cmd *cmd;
	int nongit = 0;

	/* If we're not yet authenticated, no matter what page we're on,
	 * display the authentication body from the auth_filter. This should
	 * never be cached. */
	if (!ctx.env.authenticated) {
		ctx.page.title = "Authentication Required";
		cgit_print_http_headers();
		cgit_print_docstart();
		cgit_print_pageheader();
		cgit_auth_print_body();
		cgit_print_docend();
		return;
	}

	if (ctx.repo)
		cgit_repo_setup_env(&nongit);

	cmd = cgit_get_cmd();
	if (!cmd) {
		ctx.page.title = "cgit error";
		cgit_print_error_page(404, "Not found", "Invalid request");
		return;
	}

	if (!ctx.cfg.enable_http_clone && cmd->is_clone) {
		ctx.page.title = "cgit error";
		cgit_print_error_page(404, "Not found", "Invalid request");
		return;
	}

	if (cmd->want_repo && !ctx.repo) {
		cgit_print_error_page(400, "Bad request",
				"No repository selected");
		return;
	}

	/* If cmd->want_vpath is set, assume ctx.qry.path contains a "virtual"
	 * in-project path limit to be made available at ctx.qry.vpath.
	 * Otherwise, no path limit is in effect (ctx.qry.vpath = NULL).
	 */
	ctx.qry.vpath = cmd->want_vpath ? ctx.qry.path : NULL;

	if (ctx.repo && cgit_repo_prepare_cmd(nongit))
		return;

	cmd->fn();
}

static int calc_ttl(void)
{
	if (!ctx.repo)
		return ctx.cfg.cache_root_ttl;

	if (!ctx.qry.page)
		return ctx.cfg.cache_repo_ttl;

	if (!strcmp(ctx.qry.page, "about"))
		return ctx.cfg.cache_about_ttl;

	if (!strcmp(ctx.qry.page, "snapshot"))
		return ctx.cfg.cache_snapshot_ttl;

	if (ctx.qry.has_oid)
		return ctx.cfg.cache_static_ttl;

	if (ctx.qry.has_symref)
		return ctx.cfg.cache_dynamic_ttl;

	return ctx.cfg.cache_repo_ttl;
}

int cmd_main(int argc, const char **argv)
{
	const char *path;
	int err, ttl;

	cgit_init_filters();
	atexit(cgit_cleanup_filters);

	cgit_prepare_context();
	cgit_repolist.length = 0;
	cgit_repolist.count = 0;
	cgit_repolist.repos = NULL;

	cgit_parse_args(argc, argv);
	cgit_parse_config_file(expand_macros(ctx.env.cgit_config));
	ctx.repo = NULL;
	cgit_parse_querystring();

	/* If virtual-root isn't specified in cgitrc, lets pretend
	 * that virtual-root equals SCRIPT_NAME, minus any possibly
	 * trailing slashes.
	 */
	if (!ctx.cfg.virtual_root && ctx.cfg.script_name)
		ctx.cfg.virtual_root = ensure_end(ctx.cfg.script_name, '/');

	/* If no url parameter is specified on the querystring, lets
	 * use PATH_INFO as url. This allows cgit to work with virtual
	 * urls without the need for rewriterules in the webserver (as
	 * long as PATH_INFO is included in the cache lookup key).
	 */
	path = ctx.env.path_info;
	if (!ctx.qry.url && path) {
		if (path[0] == '/')
			path++;
		ctx.qry.url = xstrdup(path);
		if (ctx.qry.raw) {
			char *newqry = fmtalloc("%s?%s", path, ctx.qry.raw);
			free(ctx.qry.raw);
			ctx.qry.raw = newqry;
		} else
			ctx.qry.raw = xstrdup(ctx.qry.url);
		cgit_parse_url(ctx.qry.url);
	}

	/* Before we go any further, we set ctx.env.authenticated by checking to see
	 * if the supplied cookie is valid. All cookies are valid if there is no
	 * auth_filter. If there is an auth_filter, the filter decides. */
	cgit_authenticate_cookie();

	ttl = calc_ttl();
	if (ttl < 0)
		ctx.page.expires += 10 * 365 * 24 * 60 * 60; /* 10 years */
	else
		ctx.page.expires += ttl * 60;
	if (!ctx.env.authenticated || (ctx.env.request_method && !strcmp(ctx.env.request_method, "HEAD")))
		ctx.cfg.cache_size = 0;
	err = cache_process(ctx.cfg.cache_size, ctx.cfg.cache_root,
			    ctx.qry.raw, ttl, process_request);
	cgit_cleanup_filters();
	if (err)
		cgit_print_error("Error processing page: %s (%d)",
				 strerror(err), err);
	return err;
}
