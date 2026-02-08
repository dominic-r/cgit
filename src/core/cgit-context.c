#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-stats.h"
#include "cgit-main.h"

void cgit_prepare_context(void)
{
	memset(&ctx, 0, sizeof(ctx));
	ctx.cfg.agefile = "info/web/last-modified";
	ctx.cfg.cache_size = 0;
	ctx.cfg.cache_max_create_time = 5;
	ctx.cfg.cache_root = CGIT_CACHE_ROOT;
	ctx.cfg.cache_about_ttl = 15;
	ctx.cfg.cache_snapshot_ttl = 5;
	ctx.cfg.cache_repo_ttl = 5;
	ctx.cfg.cache_root_ttl = 5;
	ctx.cfg.cache_scanrc_ttl = 15;
	ctx.cfg.cache_dynamic_ttl = 5;
	ctx.cfg.cache_static_ttl = -1;
	ctx.cfg.case_sensitive_sort = 1;
	ctx.cfg.branch_sort = 0;
	ctx.cfg.commit_sort = 0;
	ctx.cfg.logo = "/cgit.png";
	ctx.cfg.favicon = "/favicon.ico";
	ctx.cfg.local_time = 0;
	ctx.cfg.enable_http_clone = 1;
	ctx.cfg.enable_index_owner = 1;
	ctx.cfg.enable_tree_linenumbers = 1;
	ctx.cfg.enable_git_config = 0;
	ctx.cfg.max_repo_count = 50;
	ctx.cfg.max_commit_count = 50;
	ctx.cfg.max_lock_attempts = 5;
	ctx.cfg.max_msg_len = 80;
	ctx.cfg.max_repodesc_len = 80;
	ctx.cfg.max_blob_size = 0;
	ctx.cfg.max_stats = cgit_find_stats_period("year", NULL);
	ctx.cfg.project_list = NULL;
	ctx.cfg.renamelimit = -1;
	ctx.cfg.remove_suffix = 0;
	ctx.cfg.robots = "index, nofollow";
	ctx.cfg.root_title = "Git repository browser";
	ctx.cfg.root_desc = "a fast webinterface for the git dscm";
	ctx.cfg.scan_hidden_path = 0;
	ctx.cfg.script_name = CGIT_SCRIPT_NAME;
	ctx.cfg.section = "";
	ctx.cfg.repository_sort = "name";
	ctx.cfg.section_sort = 1;
	ctx.cfg.summary_branches = 10;
	ctx.cfg.summary_log = 10;
	ctx.cfg.summary_tags = 10;
	ctx.cfg.max_atom_items = 10;
	ctx.cfg.difftype = DIFF_UNIFIED;
	ctx.env.cgit_config = getenv("CGIT_CONFIG");
	ctx.env.http_host = getenv("HTTP_HOST");
	ctx.env.https = getenv("HTTPS");
	ctx.env.no_http = getenv("NO_HTTP");
	ctx.env.path_info = getenv("PATH_INFO");
	ctx.env.query_string = getenv("QUERY_STRING");
	ctx.env.request_method = getenv("REQUEST_METHOD");
	ctx.env.script_name = getenv("SCRIPT_NAME");
	ctx.env.server_name = getenv("SERVER_NAME");
	ctx.env.server_port = getenv("SERVER_PORT");
	ctx.env.http_cookie = getenv("HTTP_COOKIE");
	ctx.env.http_referer = getenv("HTTP_REFERER");
	ctx.env.content_length = getenv("CONTENT_LENGTH") ?
		strtoul(getenv("CONTENT_LENGTH"), NULL, 10) : 0;
	ctx.env.authenticated = 0;
	ctx.page.mimetype = "text/html";
	ctx.page.charset = PAGE_ENCODING;
	ctx.page.filename = NULL;
	ctx.page.size = 0;
	ctx.page.modified = time(NULL);
	ctx.page.expires = ctx.page.modified;
	ctx.page.etag = NULL;
	string_list_init_dup(&ctx.cfg.mimetypes);
	if (ctx.env.script_name)
		ctx.cfg.script_name = xstrdup(ctx.env.script_name);
	if (ctx.env.query_string)
		ctx.qry.raw = xstrdup(ctx.env.query_string);
	if (!ctx.env.cgit_config)
		ctx.env.cgit_config = CGIT_CONFIG;
}
