/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"
#include "html.h"
#include "ui-shared-forms.h"

static void add_clone_urls(void (*fn)(const char *), char *txt, char *suffix)
{
	struct strbuf **url_list = strbuf_split_str(txt, ' ', 0);
	int i;

	for (i = 0; url_list[i]; i++) {
		strbuf_rtrim(url_list[i]);
		if (url_list[i]->len == 0)
			continue;
		if (suffix && *suffix)
			strbuf_addf(url_list[i], "/%s", suffix);
		fn(url_list[i]->buf);
	}

	strbuf_list_free(url_list);
}

void cgit_add_clone_urls(void (*fn)(const char *))
{
	if (ctx.repo->clone_url)
		add_clone_urls(fn, expand_macros(ctx.repo->clone_url), NULL);
	else if (ctx.cfg.clone_prefix)
		add_clone_urls(fn, ctx.cfg.clone_prefix, ctx.repo->url);
}

int cgit_print_branch_option(const struct reference *ref, void *cb_data UNUSED)
{
	char *name = (char *)ref->name;
	html_option(name, name, ctx.qry.head);
	return 0;
}

void cgit_add_hidden_formfields(int incl_head, int incl_search,
				const char *page)
{
	if (!ctx.cfg.virtual_root) {
		struct strbuf url = STRBUF_INIT;

		strbuf_addf(&url, "%s/%s", ctx.qry.repo, page);
		if (ctx.qry.vpath)
			strbuf_addf(&url, "/%s", ctx.qry.vpath);
		html_hidden("url", url.buf);
		strbuf_release(&url);
	}

	if (incl_head && ctx.qry.head && ctx.repo->defbranch &&
	    strcmp(ctx.qry.head, ctx.repo->defbranch))
		html_hidden("h", ctx.qry.head);

	if (ctx.qry.oid)
		html_hidden("id", ctx.qry.oid);
	if (ctx.qry.oid2)
		html_hidden("id2", ctx.qry.oid2);
	if (ctx.qry.showmsg)
		html_hidden("showmsg", "1");

	if (incl_search) {
		if (ctx.qry.grep)
			html_hidden("qt", ctx.qry.grep);
		if (ctx.qry.search)
			html_hidden("q", ctx.qry.search);
	}
}
