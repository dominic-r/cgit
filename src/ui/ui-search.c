/* ui-search.c: file search and code search
 *
 * Copyright (C) Dominic R and contributors (see AUTHORS)
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

#define MAX_FILE_RESULTS 100
#define MAX_CODE_RESULTS 50
#define MAX_MATCHES_PER_FILE 5
#define CONTEXT_LINES 2

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
const char *cgit_strcasestr_safe(const char *haystack, const char *needle)
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

int cgit_file_search_cb(const struct object_id *oid, struct strbuf *base,
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

	if (cgit_strcasestr_safe(fullpath.buf, sctx->pattern))
		add_result(sctx, fullpath.buf, mode);

	strbuf_release(&fullpath);
	return 0;
}

int cgit_code_search_cb(const struct object_id *oid, struct strbuf *base,
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
						if (line_len > CGIT_SEARCH_MAX_LINE_LEN)
							line_len = CGIT_SEARCH_MAX_LINE_LEN;
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
	cgit_print_search_form(pattern, head, search_type);

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
		cgit_search_code(commit, &sctx);
		cgit_render_code_results(&sctx, head);
	} else {
		sctx.max_results = MAX_FILE_RESULTS;
		cgit_search_files(commit, &sctx);
		cgit_render_file_results(&sctx, head);
	}

	if (sctx.count >= sctx.max_results)
		htmlf("<div class='search-info'>Results limited to %d entries.</div>\n",
		      sctx.max_results);

	free_search_results(&sctx);
	cgit_print_layout_end();
}
