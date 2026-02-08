/* ui-diff.c: show diff between two blobs
 *
 * Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-diff.h"
#include "html.h"
#include "ui-shared.h"
#include "ui-ssdiff.h"
#include "ui-diff-render.h"

struct object_id old_rev_oid[1];
struct object_id new_rev_oid[1];

void cgit_print_diff_ctrls(void)
{
	int i, curr;

	html("<div class='cgit-panel'>");
	html("<b>diff options</b>");
	html("<form method='get'>");
	cgit_add_hidden_formfields(1, 0, ctx.qry.page);
	html("<table>");
	html("<tr><td colspan='2'/></tr>");
	html("<tr>");
	html("<td class='label'>context:</td>");
	html("<td class='ctrl'>");
	html("<select name='context' onchange='this.form.submit();'>");
	curr = ctx.qry.context;
	if (!curr)
		curr = 3;
	for (i = 1; i <= 10; i++)
		html_intoption(i, fmt("%d", i), curr);
	for (i = 15; i <= 40; i += 5)
		html_intoption(i, fmt("%d", i), curr);
	html("</select>");
	html("</td>");
	html("</tr><tr>");
	html("<td class='label'>space:</td>");
	html("<td class='ctrl'>");
	html("<select name='ignorews' onchange='this.form.submit();'>");
	html_intoption(0, "include", ctx.qry.ignorews);
	html_intoption(1, "ignore", ctx.qry.ignorews);
	html("</select>");
	html("</td>");
	html("</tr><tr>");
	html("<td class='label'>mode:</td>");
	html("<td class='ctrl'>");
	html("<select name='dt' onchange='this.form.submit();'>");
	curr = ctx.qry.has_difftype ? ctx.qry.difftype : ctx.cfg.difftype;
	html_intoption(0, "unified", curr);
	html_intoption(1, "ssdiff", curr);
	html_intoption(2, "stat only", curr);
	html("</select></td></tr>");
	html("<tr><td/><td class='ctrl'>");
	html("<noscript><input type='submit' value='reload'/></noscript>");
	html("</td></tr></table>");
	html("</form>");
	html("</div>");
}

void cgit_print_diff(const char *new_rev, const char *old_rev,
		     const char *prefix, int show_ctrls, int raw)
{
	struct commit *commit, *commit2;
	const struct object_id *old_tree_oid, *new_tree_oid;
	diff_type difftype;

	/*
	 * If "follow" is set then the diff machinery needs to examine the
	 * entire commit to detect renames so we must limit the paths in our
	 * own callbacks and not pass the prefix to the diff machinery.
	 */
	if (ctx.qry.follow && ctx.cfg.enable_follow_links) {
		cgit_diff_set_current_prefix(prefix);
		prefix = "";
	} else {
		cgit_diff_set_current_prefix(NULL);
	}

	if (!new_rev)
		new_rev = ctx.qry.head;
	if (repo_get_oid(the_repository, new_rev, new_rev_oid)) {
		cgit_print_error_page(404, "Not found",
			"Bad object name: %s", new_rev);
		return;
	}
	commit = lookup_commit_reference(the_repository, new_rev_oid);
	if (!commit || repo_parse_commit(the_repository, commit)) {
		cgit_print_error_page(404, "Not found",
			"Bad commit: %s", oid_to_hex(new_rev_oid));
		return;
	}
	new_tree_oid = get_commit_tree_oid(commit);

	if (old_rev) {
		if (repo_get_oid(the_repository, old_rev, old_rev_oid)) {
			cgit_print_error_page(404, "Not found",
				"Bad object name: %s", old_rev);
			return;
		}
	} else if (commit->parents && commit->parents->item) {
		oidcpy(old_rev_oid, &commit->parents->item->object.oid);
	} else {
		oidclr(old_rev_oid, the_repository->hash_algo);
	}

	if (!is_null_oid(old_rev_oid)) {
		commit2 = lookup_commit_reference(the_repository, old_rev_oid);
		if (!commit2 || repo_parse_commit(the_repository, commit2)) {
			cgit_print_error_page(404, "Not found",
				"Bad commit: %s", oid_to_hex(old_rev_oid));
			return;
		}
		old_tree_oid = get_commit_tree_oid(commit2);
	} else {
		old_tree_oid = NULL;
	}

	if (raw) {
		struct diff_options diffopt;

		repo_diff_setup(the_repository, &diffopt);
		diffopt.output_format = DIFF_FORMAT_PATCH;
		diffopt.flags.recursive = 1;
		diff_setup_done(&diffopt);

		ctx.page.mimetype = "text/plain";
		cgit_print_http_headers();
		if (old_tree_oid) {
			diff_tree_oid(old_tree_oid, new_tree_oid, "",
				       &diffopt);
		} else {
			diff_root_tree_oid(new_tree_oid, "", &diffopt);
		}
		diffcore_std(&diffopt);
		diff_flush(&diffopt);

		return;
	}

	difftype = ctx.qry.has_difftype ? ctx.qry.difftype : ctx.cfg.difftype;
	cgit_diff_set_use_ssdiff(difftype == DIFF_SSDIFF);

	if (show_ctrls) {
		cgit_print_layout_start();
		cgit_print_diff_ctrls();
	}

	/*
	 * Clicking on a link to a file in the diff stat should show a diff
	 * of the file, showing the diff stat limited to a single file is
	 * pretty useless.  All links from this point on will be to
	 * individual files, so we simply reset the difftype in the query
	 * here to avoid propagating DIFF_STATONLY to the individual files.
	 */
	if (difftype == DIFF_STATONLY)
		ctx.qry.difftype = ctx.cfg.difftype;

	cgit_print_diffstat(old_rev_oid, new_rev_oid, prefix);

	if (difftype == DIFF_STATONLY)
		return;

	if (difftype == DIFF_SSDIFF) {
		html("<table summary='ssdiff' class='ssdiff'>");
	} else {
		html("<table summary='diff' class='diff'>");
		html("<tr><td>");
	}
	cgit_diff_tree(old_rev_oid, new_rev_oid, cgit_diff_filepair_cb, prefix,
		       ctx.qry.ignorews);
	if (difftype != DIFF_SSDIFF)
		html("</td></tr>");
	html("</table>");

	if (show_ctrls)
		cgit_print_layout_end();
}
