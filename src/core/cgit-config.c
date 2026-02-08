/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "configfile.h"
#include "html.h"
#include "scan-tree.h"
#include "ui-stats.h"
#include "cgit-main.h"

static void add_mimetype(const char *name, const char *value)
{
	struct string_list_item *item;

	item = string_list_insert(&ctx.cfg.mimetypes, name);
	item->util = xstrdup(value);
}

void cgit_repo_config(struct cgit_repo *repo, const char *name,
		      const char *value)
{
	const char *path;
	struct string_list_item *item;

	if (!strcmp(name, "name"))
		repo->name = xstrdup(value);
	else if (!strcmp(name, "clone-url"))
		repo->clone_url = xstrdup(value);
	else if (!strcmp(name, "desc"))
		repo->desc = xstrdup(value);
	else if (!strcmp(name, "owner"))
		repo->owner = xstrdup(value);
	else if (!strcmp(name, "homepage"))
		repo->homepage = xstrdup(value);
	else if (!strcmp(name, "defbranch"))
		repo->defbranch = xstrdup(value);
	else if (!strcmp(name, "extra-head-content"))
		repo->extra_head_content = xstrdup(value);
	else if (!strcmp(name, "snapshots"))
		repo->snapshots = ctx.cfg.snapshots & cgit_parse_snapshots_mask(value);
	else if (!strcmp(name, "enable-blame"))
		repo->enable_blame = atoi(value);
	else if (!strcmp(name, "enable-commit-graph"))
		repo->enable_commit_graph = atoi(value);
	else if (!strcmp(name, "enable-log-filecount"))
		repo->enable_log_filecount = atoi(value);
	else if (!strcmp(name, "enable-log-linecount"))
		repo->enable_log_linecount = atoi(value);
	else if (!strcmp(name, "enable-remote-branches"))
		repo->enable_remote_branches = atoi(value);
	else if (!strcmp(name, "enable-subject-links"))
		repo->enable_subject_links = atoi(value);
	else if (!strcmp(name, "enable-html-serving"))
		repo->enable_html_serving = atoi(value);
	else if (!strcmp(name, "branch-sort")) {
		if (!strcmp(value, "age"))
			repo->branch_sort = 1;
		if (!strcmp(value, "name"))
			repo->branch_sort = 0;
	} else if (!strcmp(name, "commit-sort")) {
		if (!strcmp(value, "date"))
			repo->commit_sort = 1;
		if (!strcmp(value, "topo"))
			repo->commit_sort = 2;
	} else if (!strcmp(name, "max-stats")) {
		int max_stats = cgit_find_stats_period(value, NULL);
		if (max_stats)
			repo->max_stats = max_stats;
	}
	else if (!strcmp(name, "module-link"))
		repo->module_link = xstrdup(value);
	else if (skip_prefix(name, "module-link.", &path)) {
		item = string_list_append(&repo->submodules, xstrdup(path));
		item->util = xstrdup(value);
	} else if (!strcmp(name, "section"))
		repo->section = xstrdup(value);
	else if (!strcmp(name, "snapshot-prefix"))
		repo->snapshot_prefix = xstrdup(value);
	else if (!strcmp(name, "readme") && value != NULL) {
		if (repo->readme.items == ctx.cfg.readme.items)
			memset(&repo->readme, 0, sizeof(repo->readme));
		string_list_append(&repo->readme, xstrdup(value));
	} else if (!strcmp(name, "logo") && value != NULL)
		repo->logo = xstrdup(value);
	else if (!strcmp(name, "logo-link") && value != NULL)
		repo->logo_link = xstrdup(value);
	else if (!strcmp(name, "hide"))
		repo->hide = atoi(value);
	else if (!strcmp(name, "ignore"))
		repo->ignore = atoi(value);
	else if (ctx.cfg.enable_filter_overrides) {
		if (!strcmp(name, "about-filter"))
			repo->about_filter = cgit_new_filter(value, ABOUT);
		else if (!strcmp(name, "commit-filter"))
			repo->commit_filter = cgit_new_filter(value, COMMIT);
		else if (!strcmp(name, "source-filter"))
			repo->source_filter = cgit_new_filter(value, SOURCE);
		else if (!strcmp(name, "email-filter"))
			repo->email_filter = cgit_new_filter(value, EMAIL);
		else if (!strcmp(name, "owner-filter"))
			repo->owner_filter = cgit_new_filter(value, OWNER);
	}
}

static void config_cb(const char *name, const char *value)
{
	const char *arg;

	if (!strcmp(name, "section"))
		ctx.cfg.section = xstrdup(value);
	else if (!strcmp(name, "repo.url"))
		ctx.repo = cgit_add_repo(value);
	else if (ctx.repo && !strcmp(name, "repo.path"))
		ctx.repo->path = trim_end(value, '/');
	else if (ctx.repo && skip_prefix(name, "repo.", &arg))
		cgit_repo_config(ctx.repo, arg, value);
	else if (!strcmp(name, "readme"))
		string_list_append(&ctx.cfg.readme, xstrdup(value));
	else if (!strcmp(name, "root-title"))
		ctx.cfg.root_title = xstrdup(value);
	else if (!strcmp(name, "root-desc"))
		ctx.cfg.root_desc = xstrdup(value);
	else if (!strcmp(name, "root-readme"))
		ctx.cfg.root_readme = xstrdup(value);
	else if (!strcmp(name, "css"))
		string_list_append(&ctx.cfg.css, xstrdup(value));
	else if (!strcmp(name, "js"))
		string_list_append(&ctx.cfg.js, xstrdup(value));
	else if (!strcmp(name, "favicon"))
		ctx.cfg.favicon = xstrdup(value);
	else if (!strcmp(name, "footer"))
		ctx.cfg.footer = xstrdup(value);
	else if (!strcmp(name, "head-include"))
		ctx.cfg.head_include = xstrdup(value);
	else if (!strcmp(name, "header"))
		ctx.cfg.header = xstrdup(value);
	else if (!strcmp(name, "logo"))
		ctx.cfg.logo = xstrdup(value);
	else if (!strcmp(name, "logo-link"))
		ctx.cfg.logo_link = xstrdup(value);
	else if (!strcmp(name, "module-link"))
		ctx.cfg.module_link = xstrdup(value);
	else if (!strcmp(name, "strict-export"))
		ctx.cfg.strict_export = xstrdup(value);
	else if (!strcmp(name, "virtual-root"))
		ctx.cfg.virtual_root = ensure_end(value, '/');
	else if (!strcmp(name, "noplainemail"))
		ctx.cfg.noplainemail = atoi(value);
	else if (!strcmp(name, "noheader"))
		ctx.cfg.noheader = atoi(value);
	else if (!strcmp(name, "snapshots"))
		ctx.cfg.snapshots = cgit_parse_snapshots_mask(value);
	else if (!strcmp(name, "enable-filter-overrides"))
		ctx.cfg.enable_filter_overrides = atoi(value);
	else if (!strcmp(name, "enable-follow-links"))
		ctx.cfg.enable_follow_links = atoi(value);
	else if (!strcmp(name, "enable-http-clone"))
		ctx.cfg.enable_http_clone = atoi(value);
	else if (!strcmp(name, "enable-index-links"))
		ctx.cfg.enable_index_links = atoi(value);
	else if (!strcmp(name, "enable-index-owner"))
		ctx.cfg.enable_index_owner = atoi(value);
	else if (!strcmp(name, "enable-blame"))
		ctx.cfg.enable_blame = atoi(value);
	else if (!strcmp(name, "enable-commit-graph"))
		ctx.cfg.enable_commit_graph = atoi(value);
	else if (!strcmp(name, "enable-log-filecount"))
		ctx.cfg.enable_log_filecount = atoi(value);
	else if (!strcmp(name, "enable-log-linecount"))
		ctx.cfg.enable_log_linecount = atoi(value);
	else if (!strcmp(name, "enable-remote-branches"))
		ctx.cfg.enable_remote_branches = atoi(value);
	else if (!strcmp(name, "enable-subject-links"))
		ctx.cfg.enable_subject_links = atoi(value);
	else if (!strcmp(name, "enable-html-serving"))
		ctx.cfg.enable_html_serving = atoi(value);
	else if (!strcmp(name, "enable-tree-linenumbers"))
		ctx.cfg.enable_tree_linenumbers = atoi(value);
	else if (!strcmp(name, "enable-git-config"))
		ctx.cfg.enable_git_config = atoi(value);
	else if (!strcmp(name, "max-stats")) {
		int max_stats = cgit_find_stats_period(value, NULL);
		if (max_stats)
			ctx.cfg.max_stats = max_stats;
	}
	else if (!strcmp(name, "cache-size"))
		ctx.cfg.cache_size = atoi(value);
	else if (!strcmp(name, "cache-root"))
		ctx.cfg.cache_root = xstrdup(expand_macros(value));
	else if (!strcmp(name, "cache-root-ttl"))
		ctx.cfg.cache_root_ttl = atoi(value);
	else if (!strcmp(name, "cache-repo-ttl"))
		ctx.cfg.cache_repo_ttl = atoi(value);
	else if (!strcmp(name, "cache-scanrc-ttl"))
		ctx.cfg.cache_scanrc_ttl = atoi(value);
	else if (!strcmp(name, "cache-static-ttl"))
		ctx.cfg.cache_static_ttl = atoi(value);
	else if (!strcmp(name, "cache-dynamic-ttl"))
		ctx.cfg.cache_dynamic_ttl = atoi(value);
	else if (!strcmp(name, "cache-about-ttl"))
		ctx.cfg.cache_about_ttl = atoi(value);
	else if (!strcmp(name, "cache-snapshot-ttl"))
		ctx.cfg.cache_snapshot_ttl = atoi(value);
	else if (!strcmp(name, "case-sensitive-sort"))
		ctx.cfg.case_sensitive_sort = atoi(value);
	else if (!strcmp(name, "about-filter"))
		ctx.cfg.about_filter = cgit_new_filter(value, ABOUT);
	else if (!strcmp(name, "commit-filter"))
		ctx.cfg.commit_filter = cgit_new_filter(value, COMMIT);
	else if (!strcmp(name, "email-filter"))
		ctx.cfg.email_filter = cgit_new_filter(value, EMAIL);
	else if (!strcmp(name, "owner-filter"))
		ctx.cfg.owner_filter = cgit_new_filter(value, OWNER);
	else if (!strcmp(name, "auth-filter"))
		ctx.cfg.auth_filter = cgit_new_filter(value, AUTH);
	else if (!strcmp(name, "embedded"))
		ctx.cfg.embedded = atoi(value);
	else if (!strcmp(name, "max-atom-items"))
		ctx.cfg.max_atom_items = atoi(value);
	else if (!strcmp(name, "max-message-length"))
		ctx.cfg.max_msg_len = atoi(value);
	else if (!strcmp(name, "max-repodesc-length"))
		ctx.cfg.max_repodesc_len = atoi(value);
	else if (!strcmp(name, "max-blob-size"))
		ctx.cfg.max_blob_size = atoi(value);
	else if (!strcmp(name, "max-repo-count")) {
		ctx.cfg.max_repo_count = atoi(value);
		if (ctx.cfg.max_repo_count <= 0)
			ctx.cfg.max_repo_count = INT_MAX;
	} else if (!strcmp(name, "max-commit-count"))
		ctx.cfg.max_commit_count = atoi(value);
	else if (!strcmp(name, "project-list"))
		ctx.cfg.project_list = xstrdup(expand_macros(value));
	else if (!strcmp(name, "scan-path"))
		if (ctx.cfg.cache_size)
			cgit_process_cached_repolist(expand_macros(value));
		else if (ctx.cfg.project_list)
			scan_projects(expand_macros(value),
				      ctx.cfg.project_list, cgit_repo_config);
		else
			scan_tree(expand_macros(value), cgit_repo_config);
	else if (!strcmp(name, "scan-hidden-path"))
		ctx.cfg.scan_hidden_path = atoi(value);
	else if (!strcmp(name, "section-from-path"))
		ctx.cfg.section_from_path = atoi(value);
	else if (!strcmp(name, "repository-sort"))
		ctx.cfg.repository_sort = xstrdup(value);
	else if (!strcmp(name, "section-sort"))
		ctx.cfg.section_sort = atoi(value);
	else if (!strcmp(name, "source-filter"))
		ctx.cfg.source_filter = cgit_new_filter(value, SOURCE);
	else if (!strcmp(name, "summary-log"))
		ctx.cfg.summary_log = atoi(value);
	else if (!strcmp(name, "summary-branches"))
		ctx.cfg.summary_branches = atoi(value);
	else if (!strcmp(name, "summary-tags"))
		ctx.cfg.summary_tags = atoi(value);
	else if (!strcmp(name, "side-by-side-diffs"))
		ctx.cfg.difftype = atoi(value) ? DIFF_SSDIFF : DIFF_UNIFIED;
	else if (!strcmp(name, "agefile"))
		ctx.cfg.agefile = xstrdup(value);
	else if (!strcmp(name, "mimetype-file"))
		ctx.cfg.mimetype_file = xstrdup(value);
	else if (!strcmp(name, "renamelimit"))
		ctx.cfg.renamelimit = atoi(value);
	else if (!strcmp(name, "remove-suffix"))
		ctx.cfg.remove_suffix = atoi(value);
	else if (!strcmp(name, "robots"))
		ctx.cfg.robots = xstrdup(value);
	else if (!strcmp(name, "clone-prefix"))
		ctx.cfg.clone_prefix = xstrdup(value);
	else if (!strcmp(name, "clone-url"))
		ctx.cfg.clone_url = xstrdup(value);
	else if (!strcmp(name, "local-time"))
		ctx.cfg.local_time = atoi(value);
	else if (!strcmp(name, "commit-sort")) {
		if (!strcmp(value, "date"))
			ctx.cfg.commit_sort = 1;
		if (!strcmp(value, "topo"))
			ctx.cfg.commit_sort = 2;
	} else if (!strcmp(name, "branch-sort")) {
		if (!strcmp(value, "age"))
			ctx.cfg.branch_sort = 1;
		if (!strcmp(value, "name"))
			ctx.cfg.branch_sort = 0;
	} else if (skip_prefix(name, "mimetype.", &arg))
		add_mimetype(arg, value);
	else if (!strcmp(name, "include"))
		parse_configfile(expand_macros(value), config_cb);
}

static void querystring_cb(const char *name, const char *value)
{
	if (!value)
		value = "";

	if (!strcmp(name, "r")) {
		ctx.qry.repo = xstrdup(value);
		ctx.repo = cgit_get_repoinfo(value);
	} else if (!strcmp(name, "p")) {
		ctx.qry.page = xstrdup(value);
	} else if (!strcmp(name, "url")) {
		if (*value == '/')
			value++;
		ctx.qry.url = xstrdup(value);
		cgit_parse_url(value);
	} else if (!strcmp(name, "qt")) {
		ctx.qry.grep = xstrdup(value);
	} else if (!strcmp(name, "q")) {
		ctx.qry.search = xstrdup(value);
	} else if (!strcmp(name, "h")) {
		ctx.qry.head = xstrdup(value);
		ctx.qry.has_symref = 1;
	} else if (!strcmp(name, "id")) {
		ctx.qry.oid = xstrdup(value);
		ctx.qry.has_oid = 1;
	} else if (!strcmp(name, "id2")) {
		ctx.qry.oid2 = xstrdup(value);
		ctx.qry.has_oid = 1;
	} else if (!strcmp(name, "ofs")) {
		ctx.qry.ofs = atoi(value);
	} else if (!strcmp(name, "path")) {
		ctx.qry.path = trim_end(value, '/');
	} else if (!strcmp(name, "name")) {
		ctx.qry.name = xstrdup(value);
	} else if (!strcmp(name, "s")) {
		ctx.qry.sort = xstrdup(value);
	} else if (!strcmp(name, "showmsg")) {
		ctx.qry.showmsg = atoi(value);
	} else if (!strcmp(name, "period")) {
		ctx.qry.period = xstrdup(value);
	} else if (!strcmp(name, "dt")) {
		ctx.qry.difftype = atoi(value);
		ctx.qry.has_difftype = 1;
	} else if (!strcmp(name, "ss")) {
		/* No longer generated, but there may be links out there. */
		ctx.qry.difftype = atoi(value) ? DIFF_SSDIFF : DIFF_UNIFIED;
		ctx.qry.has_difftype = 1;
	} else if (!strcmp(name, "all")) {
		ctx.qry.show_all = atoi(value);
	} else if (!strcmp(name, "context")) {
		ctx.qry.context = atoi(value);
	} else if (!strcmp(name, "ignorews")) {
		ctx.qry.ignorews = atoi(value);
	} else if (!strcmp(name, "follow")) {
		ctx.qry.follow = atoi(value);
	}
}

void cgit_parse_config_file(const char *path)
{
	parse_configfile(path, config_cb);
}

void cgit_parse_querystring(void)
{
	http_parse_querystring(ctx.qry.raw, querystring_cb);
}
