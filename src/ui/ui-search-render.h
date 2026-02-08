#ifndef UI_SEARCH_RENDER_INTERNAL_H
#define UI_SEARCH_RENDER_INTERNAL_H

#include "cgit.h"

#define CGIT_SEARCH_MAX_LINE_LEN 200

struct context_line {
	int lineno;
	int is_match;
	char line[CGIT_SEARCH_MAX_LINE_LEN + 1];
};

struct code_match {
	int lineno;
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

const char *cgit_strcasestr_safe(const char *haystack, const char *needle);
int cgit_file_search_cb(const struct object_id *oid, struct strbuf *base,
			const char *pathname, unsigned mode, void *cbdata);
int cgit_code_search_cb(const struct object_id *oid, struct strbuf *base,
			const char *pathname, unsigned mode, void *cbdata);

void cgit_print_search_form(const char *pattern, const char *head,
			    const char *search_type);
void cgit_search_files(struct commit *commit, struct search_context *sctx);
void cgit_render_file_results(struct search_context *sctx, const char *head);
void cgit_search_code(struct commit *commit, struct search_context *sctx);
void cgit_render_code_results(struct search_context *sctx, const char *head);

#endif
