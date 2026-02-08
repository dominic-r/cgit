/* ui-log.c: functions for log output
 *
 * Copyright (C) 2006-2014 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-log.h"
#include "html.h"
#include "ui-shared.h"
#include "strvec.h"
#include "ui-log-commit.h"

/*
 * The list of available column colors in the commit graph.
 */
static const char *column_colors_html[] = {
	"<span class='column1'>",
	"<span class='column2'>",
	"<span class='column3'>",
	"<span class='column4'>",
	"<span class='column5'>",
	"<span class='column6'>",
	"</span>",
};

#define COLUMN_COLORS_HTML_MAX (ARRAY_SIZE(column_colors_html) - 1)

static const char *disambiguate_ref(const char *ref, int *must_free_result)
{
	struct object_id oid;
	struct strbuf longref = STRBUF_INIT;

	strbuf_addf(&longref, "refs/heads/%s", ref);
	if (repo_get_oid(the_repository, longref.buf, &oid) == 0) {
		*must_free_result = 1;
		return strbuf_detach(&longref, NULL);
	}

	*must_free_result = 0;
	strbuf_release(&longref);
	return ref;
}

static char *next_token(char **src)
{
	char *result;

	if (!src || !*src)
		return NULL;
	while (isspace(**src))
		(*src)++;
	if (!**src)
		return NULL;
	result = *src;
	while (**src) {
		if (isspace(**src)) {
			**src = '\0';
			(*src)++;
			break;
		}
		(*src)++;
	}
	return result;
}

void cgit_print_log(const char *tip, int ofs, int cnt, char *grep, char *pattern,
		    const char *path, int pager, int commit_graph, int commit_sort)
{
	struct rev_info rev;
	struct commit *commit;
	struct strvec rev_argv = STRVEC_INIT;
	int i, columns = commit_graph ? 4 : 3;
	int must_free_tip = 0;

	/* rev_argv.argv[0] will be ignored by setup_revisions */
	strvec_push(&rev_argv, "log_rev_setup");

	if (!tip)
		tip = ctx.qry.head;
	tip = disambiguate_ref(tip, &must_free_tip);
	strvec_push(&rev_argv, tip);

	if (grep && pattern && *pattern) {
		pattern = xstrdup(pattern);
		if (!strcmp(grep, "grep") || !strcmp(grep, "author") ||
		    !strcmp(grep, "committer")) {
			strvec_pushf(&rev_argv, "--%s=%s", grep, pattern);
		} else if (!strcmp(grep, "range")) {
			char *arg;
			/* Split the pattern at whitespace and add each token
			 * as a revision expression. Do not accept other
			 * rev-list options. Also, replace the previously
			 * pushed tip (it's no longer relevant).
			 */
			strvec_pop(&rev_argv);
			while ((arg = next_token(&pattern))) {
				if (*arg == '-') {
					fprintf(stderr, "Bad range expr: %s\n",
						arg);
					break;
				}
				strvec_push(&rev_argv, arg);
			}
		}
	}

	if (!path || !ctx.cfg.enable_follow_links) {
		/*
		 * If we don't have a path, "follow" is a no-op so make sure
		 * the variable is set to false to avoid needing to check
		 * both this and whether we have a path everywhere.
		 */
		ctx.qry.follow = 0;
	}

	if (commit_graph && !ctx.qry.follow) {
		strvec_push(&rev_argv, "--graph");
		strvec_push(&rev_argv, "--color");
		graph_set_column_colors(column_colors_html,
					COLUMN_COLORS_HTML_MAX);
	}

	if (commit_sort == 1)
		strvec_push(&rev_argv, "--date-order");
	else if (commit_sort == 2)
		strvec_push(&rev_argv, "--topo-order");

	if (path && ctx.qry.follow)
		strvec_push(&rev_argv, "--follow");
	strvec_push(&rev_argv, "--");
	if (path)
		strvec_push(&rev_argv, path);

	repo_init_revisions(the_repository, &rev, NULL);
	rev.abbrev = DEFAULT_ABBREV;
	rev.commit_format = CMIT_FMT_DEFAULT;
	rev.verbose_header = 1;
	rev.show_root_diff = 0;
	rev.ignore_missing = 1;
	rev.simplify_history = 1;
	setup_revisions(rev_argv.nr, rev_argv.v, &rev, NULL);
	load_ref_decorations(NULL, DECORATE_FULL_REFS);
	rev.show_decorations = 1;
	rev.grep_filter.ignore_case = 1;

	rev.diffopt.detect_rename = 1;
	rev.diffopt.rename_limit = ctx.cfg.renamelimit;
	if (ctx.qry.ignorews)
		DIFF_XDL_SET(&rev.diffopt, IGNORE_WHITESPACE);

	compile_grep_patterns(&rev.grep_filter);
	prepare_revision_walk(&rev);

	if (pager) {
		cgit_print_layout_start();
		html("<table class='list nowrap'>");
	}

	html("<tr class='nohover'>");
	if (commit_graph)
		html("<th></th>");
	else
		html("<th class='left'>Age</th>");
	html("<th class='left'>Commit message");
	if (pager) {
		html(" (");
		cgit_log_link(ctx.qry.showmsg ? "Collapse" : "Expand", NULL,
			      NULL, ctx.qry.head, ctx.qry.oid,
			      ctx.qry.vpath, ctx.qry.ofs, ctx.qry.grep,
			      ctx.qry.search, ctx.qry.showmsg ? 0 : 1,
			      ctx.qry.follow);
		html(")");
	}
	html("</th><th class='left'>Author</th>");
	if (rev.graph)
		html("<th class='left'>Age</th>");
	if (ctx.repo->enable_log_filecount) {
		html("<th class='left'>Files</th>");
		columns++;
	}
	if (ctx.repo->enable_log_linecount) {
		html("<th class='left'>Lines</th>");
		columns++;
	}
	html("</tr>\n");

	if (ofs<0)
		ofs = 0;

	for (i = 0; i < ofs && (commit = get_revision(&rev)) != NULL; /* nop */) {
		if (cgit_log_show_commit(commit, &rev))
			i++;
		release_commit_memory(the_repository->parsed_objects, commit);
		commit->parents = NULL;
	}

	for (i = 0; i < cnt && (commit = get_revision(&rev)) != NULL; /* nop */) {
		/*
		 * In "follow" mode, we must count the files and lines the
		 * first time we invoke diff on a given commit, and we need
		 * to do that to see if the commit touches the path we care
		 * about, so we do it in show_commit.  Hence we must clear
		 * lines_counted here.
		 *
		 * This has the side effect of avoiding running diff twice
		 * when we are both following renames and showing file
		 * and/or line counts.
		 */
		cgit_log_reset_lines_counted();
		if (cgit_log_show_commit(commit, &rev)) {
			i++;
			cgit_log_print_commit(commit, &rev);
		}
		release_commit_memory(the_repository->parsed_objects, commit);
		commit->parents = NULL;
	}
	if (pager) {
		html("</table><ul class='pager'>");
		if (ofs > 0) {
			html("<li>");
			cgit_log_link("[prev]", NULL, NULL, ctx.qry.head,
				      ctx.qry.oid, ctx.qry.vpath,
				      ofs - cnt, ctx.qry.grep,
				      ctx.qry.search, ctx.qry.showmsg,
				      ctx.qry.follow);
			html("</li>");
		}
		if ((commit = get_revision(&rev)) != NULL) {
			html("<li>");
			cgit_log_link("[next]", NULL, NULL, ctx.qry.head,
				      ctx.qry.oid, ctx.qry.vpath,
				      ofs + cnt, ctx.qry.grep,
				      ctx.qry.search, ctx.qry.showmsg,
				      ctx.qry.follow);
			html("</li>");
		}
		html("</ul>");
		cgit_print_layout_end();
	} else if ((commit = get_revision(&rev)) != NULL) {
		htmlf("<tr class='nohover'><td colspan='%d'>", columns);
		cgit_log_link("[...]", NULL, NULL, ctx.qry.head, NULL,
			      ctx.qry.vpath, 0, NULL, NULL, ctx.qry.showmsg,
			      ctx.qry.follow);
		html("</td></tr>\n");
	}

	/* If we allocated tip then it is safe to cast away const. */
	if (must_free_tip)
		free((char*) tip);
}
