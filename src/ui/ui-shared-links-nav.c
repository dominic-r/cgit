#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"
#include "html.h"
#include "ui-shared-links.h"

static void site_url(const char *page, const char *search, const char *sort, int ofs, int always_root)
{
	char *delim = "?";

	if (always_root || page)
		html_attr(cgit_rooturl());
	else {
		char *currenturl = cgit_currenturl();
		html_attr(currenturl);
		free(currenturl);
	}

	if (page) {
		htmlf("?p=%s", page);
		delim = "&amp;";
	}
	if (search) {
		html(delim);
		html("q=");
		html_attr(search);
		delim = "&amp;";
	}
	if (sort) {
		html(delim);
		html("s=");
		html_attr(sort);
		delim = "&amp;";
	}
	if (ofs) {
		html(delim);
		htmlf("ofs=%d", ofs);
	}
}

void cgit_site_link(const char *page, const char *name, const char *title,
		      const char *class, const char *search, const char *sort, int ofs, int always_root)
{
	html("<a");
	if (title) {
		html(" title='");
		html_attr(title);
		html("'");
	}
	if (class) {
		html(" class='");
		html_attr(class);
		html("'");
	}
	html(" href='");
	site_url(page, search, sort, ofs, always_root);
	html("'>");
	html_txt(name);
	html("</a>");
}

void cgit_index_link(const char *name, const char *title, const char *class,
		     const char *pattern, const char *sort, int ofs, int always_root)
{
	cgit_site_link(NULL, name, title, class, pattern, sort, ofs, always_root);
}

static char *repolink(const char *title, const char *class, const char *page,
		      const char *head, const char *path)
{
	char *delim = "?";

	html("<a");
	if (title) {
		html(" title='");
		html_attr(title);
		html("'");
	}
	if (class) {
		html(" class='");
		html_attr(class);
		html("'");
	}
	html(" href='");
	if (ctx.cfg.virtual_root) {
		html_url_path(ctx.cfg.virtual_root);
		html_url_path(ctx.repo->url);
		if (ctx.repo->url[strlen(ctx.repo->url) - 1] != '/')
			html("/");
		if (page) {
			html_url_path(page);
			html("/");
			if (path)
				html_url_path(path);
		}
	} else {
		html_url_path(ctx.cfg.script_name);
		html("?url=");
		html_url_arg(ctx.repo->url);
		if (ctx.repo->url[strlen(ctx.repo->url) - 1] != '/')
			html("/");
		if (page) {
			html_url_arg(page);
			html("/");
			if (path)
				html_url_arg(path);
		}
		delim = "&amp;";
	}
	if (head && ctx.repo->defbranch && strcmp(head, ctx.repo->defbranch)) {
		html(delim);
		html("h=");
		html_url_arg(head);
		delim = "&amp;";
	}
	return fmt("%s", delim);
}

void cgit_reporevlink(const char *page, const char *name, const char *title,
			const char *class, const char *head, const char *rev,
			const char *path)
{
	char *delim;

	delim = repolink(title, class, page, head, path);
	if (rev && ctx.qry.head != NULL && strcmp(rev, ctx.qry.head)) {
		html(delim);
		html("id=");
		html_url_arg(rev);
	}
	html("'>");
	html_txt(name);
	html("</a>");
}

void cgit_summary_link(const char *name, const char *title, const char *class,
		       const char *head)
{
	cgit_reporevlink(NULL, name, title, class, head, NULL, NULL);
}

void cgit_tag_link(const char *name, const char *title, const char *class,
		   const char *tag)
{
	cgit_reporevlink("tag", name, title, class, tag, NULL, NULL);
}

void cgit_tree_link(const char *name, const char *title, const char *class,
		    const char *head, const char *rev, const char *path)
{
	cgit_reporevlink("tree", name, title, class, head, rev, path);
}

void cgit_plain_link(const char *name, const char *title, const char *class,
		     const char *head, const char *rev, const char *path)
{
	cgit_reporevlink("plain", name, title, class, head, rev, path);
}

void cgit_blame_link(const char *name, const char *title, const char *class,
		     const char *head, const char *rev, const char *path)
{
	cgit_reporevlink("blame", name, title, class, head, rev, path);
}

void cgit_search_link(const char *name, const char *title, const char *class,
		      const char *head, const char *path)
{
	cgit_reporevlink("search", name, title, class, head, NULL, path);
}

void cgit_log_link(const char *name, const char *title, const char *class,
		   const char *head, const char *rev, const char *path,
		   int ofs, const char *grep, const char *pattern, int showmsg,
		   int follow)
{
	char *delim;

	delim = repolink(title, class, "log", head, path);
	if (rev && ctx.qry.head && strcmp(rev, ctx.qry.head)) {
		html(delim);
		html("id=");
		html_url_arg(rev);
		delim = "&amp;";
	}
	if (grep && pattern) {
		html(delim);
		html("qt=");
		html_url_arg(grep);
		delim = "&amp;";
		html(delim);
		html("q=");
		html_url_arg(pattern);
	}
	if (ofs > 0) {
		html(delim);
		html("ofs=");
		htmlf("%d", ofs);
		delim = "&amp;";
	}
	if (showmsg) {
		html(delim);
		html("showmsg=1");
		delim = "&amp;";
	}
	if (follow) {
		html(delim);
		html("follow=1");
	}
	html("'>");
	html_txt(name);
	html("</a>");
}

void cgit_commit_link(const char *name, const char *title, const char *class,
		      const char *head, const char *rev, const char *path)
{
	char *delim;

	delim = repolink(title, class, "commit", head, path);
	if (rev && ctx.qry.head && strcmp(rev, ctx.qry.head)) {
		html(delim);
		html("id=");
		html_url_arg(rev);
		delim = "&amp;";
	}
	if (ctx.qry.difftype) {
		html(delim);
		htmlf("dt=%d", ctx.qry.difftype);
		delim = "&amp;";
	}
	if (ctx.qry.context > 0 && ctx.qry.context != 3) {
		html(delim);
		html("context=");
		htmlf("%d", ctx.qry.context);
		delim = "&amp;";
	}
	if (ctx.qry.ignorews) {
		html(delim);
		html("ignorews=1");
		delim = "&amp;";
	}
	if (ctx.qry.follow) {
		html(delim);
		html("follow=1");
	}
	html("'>");
	if (name[0] != '\0') {
		if (strlen(name) > ctx.cfg.max_msg_len && ctx.cfg.max_msg_len >= 15) {
			html_ntxt(name, ctx.cfg.max_msg_len - 3);
			html("...");
		} else
			html_txt(name);
	} else
		html_txt("(no commit message)");
	html("</a>");
}

void cgit_refs_link(const char *name, const char *title, const char *class,
		    const char *head, const char *rev, const char *path)
{
	cgit_reporevlink("refs", name, title, class, head, rev, path);
}

void cgit_snapshot_link(const char *name, const char *title, const char *class,
			const char *head, const char *rev,
			const char *archivename)
{
	cgit_reporevlink("snapshot", name, title, class, head, rev, archivename);
}

void cgit_compare_link(const char *name, const char *title, const char *class,
		       const char *head, const char *new_rev,
		       const char *old_rev)
{
	char *delim;

	delim = repolink(title, class, "compare", head, NULL);
	if (new_rev && old_rev) {
		html(delim);
		html("id=");
		html_url_arg(old_rev);
		delim = "&amp;";
		html(delim);
		html("id2=");
		html_url_arg(new_rev);
	}
	html("'>");
	html_txt(name);
	html("</a>");
}

void cgit_diff_link(const char *name, const char *title, const char *class,
		    const char *head, const char *new_rev, const char *old_rev,
		    const char *path)
{
	char *delim;

	delim = repolink(title, class, "diff", head, path);
	if (new_rev && ctx.qry.head != NULL && strcmp(new_rev, ctx.qry.head)) {
		html(delim);
		html("id=");
		html_url_arg(new_rev);
		delim = "&amp;";
	}
	if (old_rev) {
		html(delim);
		html("id2=");
		html_url_arg(old_rev);
		delim = "&amp;";
	}
	if (ctx.qry.difftype) {
		html(delim);
		htmlf("dt=%d", ctx.qry.difftype);
		delim = "&amp;";
	}
	if (ctx.qry.context > 0 && ctx.qry.context != 3) {
		html(delim);
		html("context=");
		htmlf("%d", ctx.qry.context);
		delim = "&amp;";
	}
	if (ctx.qry.ignorews) {
		html(delim);
		html("ignorews=1");
		delim = "&amp;";
	}
	if (ctx.qry.follow) {
		html(delim);
		html("follow=1");
	}
	html("'>");
	html_txt(name);
	html("</a>");
}

void cgit_patch_link(const char *name, const char *title, const char *class,
		     const char *head, const char *rev, const char *path)
{
	cgit_reporevlink("patch", name, title, class, head, rev, path);
}

void cgit_stats_link(const char *name, const char *title, const char *class,
		     const char *head, const char *path)
{
	cgit_reporevlink("stats", name, title, class, head, NULL, path);
}

void cgit_self_link(char *name, const char *title, const char *class)
{
	if (!strcmp(ctx.qry.page, "repolist"))
		cgit_index_link(name, title, class, ctx.qry.search, ctx.qry.sort,
				ctx.qry.ofs, 1);
	else if (!strcmp(ctx.qry.page, "summary"))
		cgit_summary_link(name, title, class, ctx.qry.head);
	else if (!strcmp(ctx.qry.page, "tag"))
		cgit_tag_link(name, title, class, ctx.qry.has_oid ?
			       ctx.qry.oid : ctx.qry.head);
	else if (!strcmp(ctx.qry.page, "tree"))
		cgit_tree_link(name, title, class, ctx.qry.head,
			       ctx.qry.has_oid ? ctx.qry.oid : NULL,
			       ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "plain"))
		cgit_plain_link(name, title, class, ctx.qry.head,
				ctx.qry.has_oid ? ctx.qry.oid : NULL,
				ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "blame"))
		cgit_blame_link(name, title, class, ctx.qry.head,
				ctx.qry.has_oid ? ctx.qry.oid : NULL,
				ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "log"))
		cgit_log_link(name, title, class, ctx.qry.head,
			      ctx.qry.has_oid ? ctx.qry.oid : NULL,
			      ctx.qry.path, ctx.qry.ofs,
			      ctx.qry.grep, ctx.qry.search,
			      ctx.qry.showmsg, ctx.qry.follow);
	else if (!strcmp(ctx.qry.page, "commit"))
		cgit_commit_link(name, title, class, ctx.qry.head,
				 ctx.qry.has_oid ? ctx.qry.oid : NULL,
				 ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "patch"))
		cgit_patch_link(name, title, class, ctx.qry.head,
				ctx.qry.has_oid ? ctx.qry.oid : NULL,
				ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "refs"))
		cgit_refs_link(name, title, class, ctx.qry.head,
			       ctx.qry.has_oid ? ctx.qry.oid : NULL,
			       ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "snapshot"))
		cgit_snapshot_link(name, title, class, ctx.qry.head,
				   ctx.qry.has_oid ? ctx.qry.oid : NULL,
				   ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "compare"))
		cgit_compare_link(name, title, class, ctx.qry.head,
				  ctx.qry.oid, ctx.qry.oid2);
	else if (!strcmp(ctx.qry.page, "diff"))
		cgit_diff_link(name, title, class, ctx.qry.head,
			       ctx.qry.oid, ctx.qry.oid2,
			       ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "stats"))
		cgit_stats_link(name, title, class, ctx.qry.head,
				ctx.qry.path);
	else if (!strcmp(ctx.qry.page, "search"))
		cgit_search_link(name, title, class, ctx.qry.head,
				 ctx.qry.path);
	else {
		/* Don't known how to make link for this page */
		repolink(title, class, ctx.qry.page, ctx.qry.head, ctx.qry.path);
		html("><!-- cgit_self_link() doesn't know how to make link for page '");
		html_txt(ctx.qry.page);
		html("' -->");
		html_txt(name);
		html("</a>");
	}
}
