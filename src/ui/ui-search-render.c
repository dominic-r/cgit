/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-search.h"
#include "html.h"
#include "ui-shared.h"
#include "ui-search-render.h"

void cgit_print_search_form(const char *pattern, const char *head,
			      const char *search_type)
{
	html("<form class='search-form' method='get'");
	if (ctx.cfg.virtual_root) {
		char *fileurl = cgit_fileurl(ctx.qry.repo, "search",
					    NULL, NULL);
		html(" action='");
		html_url_path(fileurl);
		html("'");
		free(fileurl);
	}
	html(">\n");
	cgit_add_hidden_formfields(1, 0, "search");
	html("<select name='qt'>\n");
	html_option("file", "file name", search_type);
	html_option("code", "code", search_type);
	html("</select>\n");
	html("<input class='txt' type='search' size='30' name='q' value='");
	html_attr(pattern ? pattern : "");
	html("'/>\n");
	html("<input type='submit' value='search'/>\n");
	html("</form>\n");
}

void cgit_search_files(struct commit *commit, struct search_context *sctx)
{
	struct tree *tree;
	struct pathspec paths = { .nr = 0 };

	tree = repo_get_commit_tree(the_repository, commit);
	if (!tree)
		return;

	read_tree(the_repository, tree, &paths, cgit_file_search_cb, sctx);
}

void cgit_render_file_results(struct search_context *sctx, const char *head)
{
	int i;

	if (!sctx->count) {
		html("<div class='search-info'>No files found.</div>\n");
		return;
	}

	htmlf("<div class='search-info'>%d file%s found</div>\n",
	      sctx->count, sctx->count == 1 ? "" : "s");

	html("<table class='list search-results'>\n");
	html("<tr class='nohover'>");
	html("<th class='left'>Path</th>");
	html("</tr>\n");

	for (i = 0; i < sctx->count; i++) {
		html("<tr><td class='search-path'>");
		cgit_tree_link(sctx->results[i].path, NULL, NULL,
			       head, NULL, sctx->results[i].path);
		html("</td></tr>\n");
	}
	html("</table>\n");
}

void cgit_search_code(struct commit *commit, struct search_context *sctx)
{
	struct tree *tree;
	struct pathspec paths = { .nr = 0 };
	struct grep_opt opt;
	struct code_search_ctx cs_ctx;

	tree = repo_get_commit_tree(the_repository, commit);
	if (!tree)
		return;

	grep_init(&opt, the_repository);
	opt.ignore_case = 1;
	opt.status_only = 1;
	append_grep_pattern(&opt, sctx->pattern, "search", 0, GREP_PATTERN);
	compile_grep_patterns(&opt);

	cs_ctx.sctx = sctx;
	cs_ctx.opt = &opt;

	read_tree(the_repository, tree, &paths, cgit_code_search_cb, &cs_ctx);

	free_grep_patterns(&opt);
}

static void html_txt_highlighted(const char *txt, const char *pattern)
{
	size_t plen = strlen(pattern);
	const char *match;

	while (*txt) {
		match = cgit_strcasestr_safe(txt, pattern);
		if (!match) {
			html_txt(txt);
			return;
		}
		html_ntxt(txt, match - txt);
		html("<span class='search-match'>");
		html_ntxt(match, plen);
		html("</span>");
		txt = match + plen;
	}
}

void cgit_render_code_results(struct search_context *sctx, const char *head)
{
	int i, j;

	if (!sctx->count) {
		html("<div class='search-info'>No results found.</div>\n");
		return;
	}

	htmlf("<div class='search-info'>Found matches in %d file%s</div>\n",
	      sctx->count, sctx->count == 1 ? "" : "s");

	html("<table class='list search-results'>\n");

	for (i = 0; i < sctx->count; i++) {
		html("<tr class='search-file nohover'><td colspan='2'>");
		cgit_tree_link(sctx->results[i].path, NULL, NULL,
			       head, NULL, sctx->results[i].path);
		html("</td></tr>\n");

		for (j = 0; j < sctx->results[i].match_count; j++) {
			struct code_match *m = &sctx->results[i].matches[j];
			int k;

			/* Separator between match groups */
			if (j > 0)
				html("<tr class='nohover'><td colspan='2' class='search-sep'></td></tr>\n");

			for (k = 0; k < m->context_count; k++) {
				struct context_line *cl = &m->context[k];
				const char *cls = cl->is_match ? "search-line match" : "search-line context";

				htmlf("<tr class='nohover'><td class='search-lineno'>");
				htmlf("<a href='");
				if (ctx.cfg.virtual_root) {
					html_url_path(ctx.cfg.virtual_root);
					html_url_path(ctx.repo->url);
					if (ctx.repo->url[strlen(ctx.repo->url) - 1] != '/')
						html("/");
					html_url_path("tree");
					html("/");
					html_url_path(sctx->results[i].path);
				} else {
					html_url_path(ctx.cfg.script_name);
					html("?url=");
					html_url_arg(ctx.repo->url);
					html("/tree/");
					html_url_arg(sctx->results[i].path);
				}
				htmlf("#n%d'>%d</a>", cl->lineno, cl->lineno);
				htmlf("</td><td class='%s'>", cls);
				if (cl->is_match)
					html_txt_highlighted(cl->line, sctx->pattern);
				else
					html_txt(cl->line);
				html("</td></tr>\n");
			}
		}
	}
	html("</table>\n");
}
