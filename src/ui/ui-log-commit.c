#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-log.h"
#include "html.h"
#include "ui-shared.h"
#include "ui-log-commit.h"

static int files, add_lines, rem_lines, lines_counted;

void cgit_log_reset_lines_counted(void)
{
	lines_counted = 0;
}

static void count_lines(char *line, int size)
{
	if (size <= 0)
		return;

	if (line[0] == '+')
		add_lines++;

	else if (line[0] == '-')
		rem_lines++;
}

static void inspect_files(struct diff_filepair *pair)
{
	unsigned long old_size = 0;
	unsigned long new_size = 0;
	int binary = 0;

	files++;
	if (ctx.repo->enable_log_linecount)
		cgit_diff_files(&pair->one->oid, &pair->two->oid, &old_size,
				&new_size, &binary, 0, ctx.qry.ignorews,
				count_lines);
}

void show_commit_decorations(struct commit *commit)
{
	const struct name_decoration *deco;
	static char buf[1024];

	buf[sizeof(buf) - 1] = 0;
	deco = get_name_decoration(&commit->object);
	if (!deco)
		return;
	html("<span class='decoration'>");
	while (deco) {
		struct object_id oid_tag, peeled;
		int is_annotated = 0;

		strlcpy(buf, prettify_refname(deco->name), sizeof(buf));
		switch(deco->type) {
		case DECORATION_NONE:
			/* If the git-core doesn't recognize it,
			 * don't display anything. */
			break;
		case DECORATION_REF_LOCAL:
			cgit_log_link(buf, NULL, "branch-deco", buf, NULL,
				ctx.qry.vpath, 0, NULL, NULL,
				ctx.qry.showmsg, 0);
			break;
		case DECORATION_REF_TAG:
			if (!refs_read_ref(get_main_ref_store(the_repository), deco->name, &oid_tag) &&
			    !peel_object(the_repository, &oid_tag, &peeled,
					 PEEL_OBJECT_VERIFY_TAGGED_OBJECT_TYPE))
				is_annotated = !oideq(&oid_tag, &peeled);
			cgit_tag_link(buf, NULL, is_annotated ? "tag-annotated-deco" : "tag-deco", buf);
			break;
		case DECORATION_REF_REMOTE:
			if (!ctx.repo->enable_remote_branches)
				break;
			cgit_log_link(buf, NULL, "remote-deco", NULL,
				oid_to_hex(&commit->object.oid),
				ctx.qry.vpath, 0, NULL, NULL,
				ctx.qry.showmsg, 0);
			break;
		default:
			cgit_commit_link(buf, NULL, "deco", ctx.qry.head,
					oid_to_hex(&commit->object.oid),
					ctx.qry.vpath);
			break;
		}
		deco = deco->next;
	}
	html("</span>");
}

static void handle_rename(struct diff_filepair *pair)
{
	/*
	 * After we have seen a rename, we generate links to the previous
	 * name of the file so that commit & diff views get fed the path
	 * that is correct for the commit they are showing, avoiding the
	 * need to walk the entire history leading back to every commit we
	 * show in order detect renames.
	 */
	if (0 != strcmp(ctx.qry.vpath, pair->two->path)) {
		free(ctx.qry.vpath);
		ctx.qry.vpath = xstrdup(pair->two->path);
	}
	inspect_files(pair);
}

int cgit_log_show_commit(struct commit *commit, struct rev_info *revs)
{
	struct commit_list *parents = commit->parents;
	struct commit *parent;
	int found = 0, saved_fmt;
	struct diff_flags saved_flags = revs->diffopt.flags;

	/* Always show if we're not in "follow" mode with a single file. */
	if (!ctx.qry.follow)
		return 1;

	/*
	 * In "follow" mode, we don't show merges.  This is consistent with
	 * "git log --follow -- <file>".
	 */
	if (parents && parents->next)
		return 0;

	/*
	 * If this is the root commit, do what rev_info tells us.
	 */
	if (!parents)
		return revs->show_root_diff;

	/* When we get here we have precisely one parent. */
	parent = parents->item;
	/* If we can't parse the commit, let print_commit() report an error. */
	if (repo_parse_commit(the_repository, parent))
		return 1;

	files = 0;
	add_lines = 0;
	rem_lines = 0;

	revs->diffopt.flags.recursive = 1;
	diff_tree_oid(get_commit_tree_oid(parent),
		      get_commit_tree_oid(commit),
		      "", &revs->diffopt);
	diffcore_std(&revs->diffopt);

	found = !diff_queue_is_empty(&revs->diffopt);
	saved_fmt = revs->diffopt.output_format;
	revs->diffopt.output_format = DIFF_FORMAT_CALLBACK;
	revs->diffopt.format_callback = cgit_diff_tree_cb;
	revs->diffopt.format_callback_data = handle_rename;
	diff_flush(&revs->diffopt);
	revs->diffopt.output_format = saved_fmt;
	revs->diffopt.flags = saved_flags;

	lines_counted = 1;
	return found;
}

void cgit_log_print_commit(struct commit *commit, struct rev_info *revs)
{
	struct commitinfo *info;
	int columns = revs->graph ? 4 : 3;
	struct strbuf graphbuf = STRBUF_INIT;
	struct strbuf msgbuf = STRBUF_INIT;

	if (ctx.repo->enable_log_filecount)
		columns++;
	if (ctx.repo->enable_log_linecount)
		columns++;

	if (revs->graph) {
		/* Advance graph until current commit */
		while (!graph_next_line(revs->graph, &graphbuf)) {
			/* Print graph segment in otherwise empty table row */
			html("<tr class='nohover'><td class='commitgraph'>");
			html(graphbuf.buf);
			htmlf("</td><td colspan='%d' /></tr>\n", columns);
			strbuf_setlen(&graphbuf, 0);
		}
		/* Current commit's graph segment is now ready in graphbuf */
	}

	info = cgit_parse_commit(commit);
	htmlf("<tr%s>", ctx.qry.showmsg ? " class='logheader'" : "");

	if (revs->graph) {
		/* Print graph segment for current commit */
		html("<td class='commitgraph'>");
		html(graphbuf.buf);
		html("</td>");
		strbuf_setlen(&graphbuf, 0);
	}
	else {
		html("<td>");
		cgit_print_age(info->committer_date, info->committer_tz, TM_WEEK * 2);
		html("</td>");
	}

	htmlf("<td%s>", ctx.qry.showmsg ? " class='logsubject'" : "");
	if (ctx.qry.showmsg) {
		/* line-wrap long commit subjects instead of truncating them */
		size_t subject_len = strlen(info->subject);

		if (subject_len > ctx.cfg.max_msg_len &&
		    ctx.cfg.max_msg_len >= 15) {
			/* symbol for signaling line-wrap (in PAGE_ENCODING) */
			const char wrap_symbol[] = { ' ', 0xE2, 0x86, 0xB5, 0 };
			int i = ctx.cfg.max_msg_len - strlen(wrap_symbol);

			/* Rewind i to preceding space character */
			while (i > 0 && !isspace(info->subject[i]))
				--i;
			if (!i) /* Oops, zero spaces. Reset i */
				i = ctx.cfg.max_msg_len - strlen(wrap_symbol);

			/* add remainder starting at i to msgbuf */
			strbuf_add(&msgbuf, info->subject + i, subject_len - i);
			strbuf_trim(&msgbuf);
			strbuf_add(&msgbuf, "\n\n", 2);

			/* Place wrap_symbol at position i in info->subject */
			strlcpy(info->subject + i, wrap_symbol, subject_len - i + 1);
		}
	}
	cgit_commit_link(info->subject, NULL, NULL, ctx.qry.head,
			 oid_to_hex(&commit->object.oid), ctx.qry.vpath);
	show_commit_decorations(commit);
	html("</td><td>");
	cgit_open_filter(ctx.repo->email_filter, info->author_email, "log");
	html_txt(info->author);
	cgit_close_filter(ctx.repo->email_filter);

	if (revs->graph) {
		html("</td><td>");
		cgit_print_age(info->committer_date, info->committer_tz, TM_WEEK * 2);
	}

	if (!lines_counted && (ctx.repo->enable_log_filecount ||
			       ctx.repo->enable_log_linecount)) {
		files = 0;
		add_lines = 0;
		rem_lines = 0;
		cgit_diff_commit(commit, inspect_files, ctx.qry.vpath);
	}

	if (ctx.repo->enable_log_filecount)
		htmlf("</td><td>%d", files);
	if (ctx.repo->enable_log_linecount)
		htmlf("</td><td><span class='deletions'>-%d</span>/"
			"<span class='insertions'>+%d</span>", rem_lines, add_lines);

	html("</td></tr>\n");

	if ((revs->graph && !graph_is_commit_finished(revs->graph))
			|| ctx.qry.showmsg) { /* Print a second table row */
		html("<tr class='nohover-highlight'>");

		if (ctx.qry.showmsg) {
			/* Concatenate commit message + notes in msgbuf */
			if (info->msg && *(info->msg)) {
				strbuf_addstr(&msgbuf, info->msg);
				strbuf_addch(&msgbuf, '\n');
			}
			format_display_notes(&commit->object.oid,
					     &msgbuf, PAGE_ENCODING, 0);
			strbuf_addch(&msgbuf, '\n');
			strbuf_ltrim(&msgbuf);
		}

		if (revs->graph) {
			int lines = 0;

			/* Calculate graph padding */
			if (ctx.qry.showmsg) {
				/* Count #lines in commit message + notes */
				const char *p = msgbuf.buf;
				lines = 1;
				while ((p = strchr(p, '\n'))) {
					p++;
					lines++;
				}
			}

			/* Print graph padding */
			html("<td class='commitgraph'>");
			while (lines > 0 || !graph_is_commit_finished(revs->graph)) {
				if (graphbuf.len)
					html("\n");
				strbuf_setlen(&graphbuf, 0);
				graph_next_line(revs->graph, &graphbuf);
				html(graphbuf.buf);
				lines--;
			}
			html("</td>\n");
		}
		else
			html("<td/>"); /* Empty 'Age' column */

		/* Print msgbuf into remainder of table row */
		htmlf("<td colspan='%d'%s>\n", columns - (revs->graph ? 1 : 0),
			ctx.qry.showmsg ? " class='logmsg'" : "");
		html_txt(msgbuf.buf);
		html("</td></tr>\n");
	}

	strbuf_release(&msgbuf);
	strbuf_release(&graphbuf);
	cgit_free_commitinfo(info);
}
