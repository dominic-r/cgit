/* ui-search.c: file search and code search
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-search.h"
#include "html.h"
#include "ui-shared.h"

#define MAX_FILE_RESULTS 100
#define MAX_CODE_RESULTS 50
#define MAX_MATCHES_PER_FILE 5
#define CONTEXT_LINES 2
#define MAX_LINE_LEN 200

struct context_line {
	int lineno;
	int is_match;
	char line[MAX_LINE_LEN + 1];
};

struct code_match {
	int lineno;		/* line number of the match itself */
	struct context_line *context;
	int context_count;
};

struct search_result {
	char *path;
	unsigned short mode;
	struct code_match *matches;
	int match_count;
};

struct search_context {
	const char *pattern;
	struct search_result *results;
	int count;
	int alloc;
	int max_results;
};

struct code_search_ctx {
	struct search_context *sctx;
	struct grep_opt *opt;
};

static void free_search_results(struct search_context *sctx)
{
	int i, j;
	for (i = 0; i < sctx->count; i++) {
		free(sctx->results[i].path);
		if (sctx->results[i].matches) {
			for (j = 0; j < sctx->results[i].match_count; j++)
				free(sctx->results[i].matches[j].context);
			free(sctx->results[i].matches);
		}
	}
	free(sctx->results);
}

static void add_result(struct search_context *sctx, const char *path,
		       unsigned short mode)
{
	if (sctx->count >= sctx->max_results)
		return;
	ALLOC_GROW(sctx->results, sctx->count + 1, sctx->alloc);
	sctx->results[sctx->count].path = xstrdup(path);
	sctx->results[sctx->count].mode = mode;
	sctx->results[sctx->count].matches = NULL;
	sctx->results[sctx->count].match_count = 0;
	sctx->count++;
}

/* Case-insensitive substring search */
static const char *strcasestr_safe(const char *haystack, const char *needle)
{
	size_t nlen = strlen(needle);
	if (!nlen)
		return haystack;
	while (*haystack) {
		if (!strncasecmp(haystack, needle, nlen))
			return haystack;
		haystack++;
	}
	return NULL;
}

/* Bounded case-insensitive substring search within a line */
static const char *strcasestr_bounded(const char *haystack, const char *haystack_end,
				      const char *needle)
{
	size_t nlen = strlen(needle);
	if (!nlen)
		return haystack;
	while (haystack + nlen <= haystack_end) {
		if (!strncasecmp(haystack, needle, nlen))
			return haystack;
		haystack++;
	}
	return NULL;
}

static int file_search_cb(const struct object_id *oid, struct strbuf *base,
			   const char *pathname, unsigned mode, void *cbdata)
{
	struct search_context *sctx = cbdata;
	struct strbuf fullpath = STRBUF_INIT;

	if (S_ISDIR(mode))
		return READ_TREE_RECURSIVE;

	if (!S_ISREG(mode) && !S_ISLNK(mode))
		return 0;

	if (sctx->count >= sctx->max_results)
		return -1;

	strbuf_addbuf(&fullpath, base);
	strbuf_addstr(&fullpath, pathname);

	if (strcasestr_safe(fullpath.buf, sctx->pattern))
		add_result(sctx, fullpath.buf, mode);

	strbuf_release(&fullpath);
	return 0;
}

static int code_search_cb(const struct object_id *oid, struct strbuf *base,
			   const char *pathname, unsigned mode, void *cbdata)
{
	struct code_search_ctx *cs = cbdata;
	struct strbuf fullpath = STRBUF_INIT;
	struct grep_source gs;
	enum object_type type;
	unsigned long size;
	char *buf;

	if (S_ISDIR(mode))
		return READ_TREE_RECURSIVE;

	if (!S_ISREG(mode))
		return 0;

	if (cs->sctx->count >= cs->sctx->max_results)
		return -1;

	strbuf_addbuf(&fullpath, base);
	strbuf_addstr(&fullpath, pathname);

	/* Use grep API to check for match */
	grep_source_init_oid(&gs, fullpath.buf, fullpath.buf,
			     oid, the_repository);

	if (grep_source(cs->opt, &gs) > 0) {
		/* Match found - read blob and extract lines */
		buf = odb_read_object(the_repository->objects,
				      oid, &type, &size);
		if (buf && !buffer_is_binary(buf, size)) {
			/* Build a line index for the whole file */
			const char **line_starts;
			const char **line_ends;
			int total_lines = 0;
			int line_alloc = 256;
			const char *p = buf;
			int match_linenos[MAX_MATCHES_PER_FILE];
			int match_count = 0;
			int i;

			line_starts = xmalloc(line_alloc * sizeof(const char *));
			line_ends = xmalloc(line_alloc * sizeof(const char *));

			/* Index all lines */
			while (p < buf + size) {
				const char *eol = memchr(p, '\n', buf + size - p);
				if (!eol)
					eol = buf + size;
				ALLOC_GROW(line_starts, total_lines + 1, line_alloc);
				/* line_ends realloc must track same alloc */
				REALLOC_ARRAY(line_ends, line_alloc);
				line_starts[total_lines] = p;
				line_ends[total_lines] = eol;
				total_lines++;
				p = eol + 1;
			}

			/* Find matching lines */
			for (i = 0; i < total_lines && match_count < MAX_MATCHES_PER_FILE; i++) {
				if (strcasestr_bounded(line_starts[i], line_ends[i],
						       cs->sctx->pattern))
					match_linenos[match_count++] = i;
			}

			if (match_count > 0) {
				struct code_match *cm = xcalloc(match_count,
							       sizeof(struct code_match));

				for (i = 0; i < match_count; i++) {
					int mi = match_linenos[i];
					int ctx_start = mi - CONTEXT_LINES;
					int ctx_end = mi + CONTEXT_LINES;
					int ctx_count, k;

					if (ctx_start < 0)
						ctx_start = 0;
					if (ctx_end >= total_lines)
						ctx_end = total_lines - 1;
					ctx_count = ctx_end - ctx_start + 1;

					cm[i].lineno = mi + 1; /* 1-based */
					cm[i].context = xcalloc(ctx_count,
								sizeof(struct context_line));
					cm[i].context_count = ctx_count;

					for (k = 0; k < ctx_count; k++) {
						int li = ctx_start + k;
						size_t line_len = line_ends[li] - line_starts[li];

						cm[i].context[k].lineno = li + 1;
						cm[i].context[k].is_match = (li == mi);
						if (line_len > MAX_LINE_LEN)
							line_len = MAX_LINE_LEN;
						memcpy(cm[i].context[k].line,
						       line_starts[li], line_len);
						cm[i].context[k].line[line_len] = '\0';
					}
				}

				add_result(cs->sctx, fullpath.buf, mode);
				cs->sctx->results[cs->sctx->count - 1].matches = cm;
				cs->sctx->results[cs->sctx->count - 1].match_count = match_count;
			}

			free(line_starts);
			free(line_ends);
		}
		free(buf);
	}

	grep_source_clear(&gs);
	strbuf_release(&fullpath);
	return 0;
}

static void print_search_form(const char *pattern, const char *head,
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

static void search_files(struct commit *commit, struct search_context *sctx)
{
	struct tree *tree;
	struct pathspec paths = { .nr = 0 };

	tree = repo_get_commit_tree(the_repository, commit);
	if (!tree)
		return;

	read_tree(the_repository, tree, &paths, file_search_cb, sctx);
}

static void render_file_results(struct search_context *sctx, const char *head)
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

static void search_code(struct commit *commit, struct search_context *sctx)
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

	read_tree(the_repository, tree, &paths, code_search_cb, &cs_ctx);

	free_grep_patterns(&opt);
}

static void html_txt_highlighted(const char *txt, const char *pattern)
{
	size_t plen = strlen(pattern);
	const char *match;

	while (*txt) {
		match = strcasestr_safe(txt, pattern);
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

static void render_code_results(struct search_context *sctx, const char *head)
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

void cgit_print_search(const char *pattern, const char *head,
		       const char *search_type)
{
	struct object_id oid;
	struct commit *commit;
	struct search_context sctx = { 0 };
	int is_code;

	if (!head)
		head = ctx.qry.head;
	if (!head)
		head = ctx.repo->defbranch;

	cgit_print_layout_start();
	print_search_form(pattern, head, search_type);

	if (!pattern || !*pattern) {
		cgit_print_layout_end();
		return;
	}

	if (repo_get_oid(the_repository, head, &oid)) {
		cgit_print_error("Invalid revision: %s", head);
		cgit_print_layout_end();
		return;
	}

	commit = lookup_commit_reference(the_repository, &oid);
	if (!commit || repo_parse_commit(the_repository, commit)) {
		cgit_print_error("Invalid commit: %s", head);
		cgit_print_layout_end();
		return;
	}

	is_code = search_type && !strcmp(search_type, "code");

	sctx.pattern = pattern;
	if (is_code) {
		sctx.max_results = MAX_CODE_RESULTS;
		search_code(commit, &sctx);
		render_code_results(&sctx, head);
	} else {
		sctx.max_results = MAX_FILE_RESULTS;
		search_files(commit, &sctx);
		render_file_results(&sctx, head);
	}

	if (sctx.count >= sctx.max_results)
		htmlf("<div class='search-info'>Results limited to %d entries.</div>\n",
		      sctx.max_results);

	free_search_results(&sctx);
	cgit_print_layout_end();
}
