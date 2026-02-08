/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "html.h"
#include "ui-blob.h"
#include "ui-shared.h"
#include "cgit-main.h"

struct refmatch {
	char *req_ref;
	char *first_ref;
	int match;
};

static int find_current_ref(const struct reference *ref, void *cb_data)
{
	struct refmatch *info;

	info = (struct refmatch *)cb_data;
	if (!strcmp(ref->name, info->req_ref))
		info->match = 1;
	if (!info->first_ref)
		info->first_ref = xstrdup(ref->name);
	return info->match;
}

static void free_refmatch_inner(struct refmatch *info)
{
	if (info->first_ref)
		free(info->first_ref);
}

static char *find_default_branch(struct cgit_repo *repo)
{
	struct refmatch info;
	char *ref;

	info.req_ref = repo->defbranch;
	info.first_ref = NULL;
	info.match = 0;
	refs_for_each_branch_ref(get_main_ref_store(the_repository),
				 find_current_ref, &info);
	if (info.match)
		ref = info.req_ref;
	else
		ref = info.first_ref;
	if (ref)
		ref = xstrdup(ref);
	free_refmatch_inner(&info);

	return ref;
}

static char *guess_defbranch(void)
{
	const char *ref, *refname;
	struct object_id oid;

	ref = refs_resolve_ref_unsafe(get_main_ref_store(the_repository),
				     "HEAD", 0, &oid, NULL);
	if (!ref || !skip_prefix(ref, "refs/heads/", &refname))
		return "master";
	return xstrdup(refname);
}

/* The caller must free filename and ref after calling this. */
static inline void parse_readme(const char *readme, char **filename,
				char **ref, struct cgit_repo *repo)
{
	const char *colon;

	*filename = NULL;
	*ref = NULL;

	if (!readme || !readme[0])
		return;

	/* Check if the readme is tracked in the git repo. */
	colon = strchr(readme, ':');
	if (colon && strlen(colon) > 1) {
		/* If it starts with a colon, we want to use head given
		 * from query or the default branch */
		if (colon == readme && ctx.qry.head)
			*ref = xstrdup(ctx.qry.head);
		else if (colon == readme && repo->defbranch)
			*ref = xstrdup(repo->defbranch);
		else
			*ref = xstrndup(readme, colon - readme);
		readme = colon + 1;
	}

	/* Prepend repo path to relative readme path unless tracked. */
	if (!(*ref) && readme[0] != '/')
		*filename = fmtalloc("%s/%s", repo->path, readme);
	else
		*filename = xstrdup(readme);
}

static void choose_readme(struct cgit_repo *repo)
{
	int found;
	char *filename, *ref;
	struct string_list_item *entry;

	if (!repo->readme.nr)
		return;

	found = 0;
	for_each_string_list_item(entry, &repo->readme) {
		parse_readme(entry->string, &filename, &ref, repo);
		if (!filename) {
			free(filename);
			free(ref);
			continue;
		}
		if (ref) {
			if (cgit_ref_path_exists(filename, ref, 1)) {
				found = 1;
				break;
			}
		}
		else if (!access(filename, R_OK)) {
			found = 1;
			break;
		}
		free(filename);
		free(ref);
	}
	repo->readme.strdup_strings = 1;
	string_list_clear(&repo->readme, 0);
	repo->readme.strdup_strings = 0;
	if (found)
		string_list_append(&repo->readme, filename)->util = ref;
}

static void print_no_repo_clone_urls(const char *url)
{
	html("<tr><td><a rel='vcs-git' href='");
	html_url_path(url);
	html("' title='");
	html_attr(ctx.repo->name);
	html(" Git repository'>");
	html_txt(url);
	html("</a></td></tr>\n");
}

void cgit_repo_setup_env(int *nongit)
{
	/* The path to the git repository. */
	setenv("GIT_DIR", ctx.repo->path, 1);

	/* Setup the git directory and initialize the notes system. Both of these
	 * load local configuration from the git repository, so we do them both while
	 * the HOME variables are unset. */
	setup_git_directory_gently(nongit);
	load_display_notes(NULL);
}

int cgit_repo_prepare_cmd(int nongit)
{
	struct object_id oid;
	int rc;

	if (nongit) {
		const char *name = ctx.repo->name;
		rc = errno;
		ctx.page.title = fmtalloc("%s - %s", ctx.cfg.root_title,
					"config error");
		ctx.repo = NULL;
		cgit_print_http_headers();
		cgit_print_docstart();
		cgit_print_pageheader();
		cgit_print_error("Failed to open %s: %s", name,
				 rc ? strerror(rc) : "Not a valid git repository");
		cgit_print_docend();
		return 1;
	}
	ctx.page.title = fmtalloc("%s - %s", ctx.repo->name, ctx.repo->desc);

	if (!ctx.repo->defbranch)
		ctx.repo->defbranch = guess_defbranch();

	if (!ctx.qry.head) {
		ctx.qry.nohead = 1;
		ctx.qry.head = find_default_branch(ctx.repo);
	}

	if (!ctx.qry.head) {
		cgit_print_http_headers();
		cgit_print_docstart();
		cgit_print_pageheader();
		cgit_print_error("Repository seems to be empty");
		if (!strcmp(ctx.qry.page, "summary")) {
			html("<table class='list'><tr class='nohover'><td>&nbsp;</td></tr><tr class='nohover'><th class='left'>Clone</th></tr>\n");
			cgit_prepare_repo_env(ctx.repo);
			cgit_add_clone_urls(print_no_repo_clone_urls);
			html("</table>\n");
		}
		cgit_print_docend();
		return 1;
	}

	if (repo_get_oid(the_repository, ctx.qry.head, &oid)) {
		char *old_head = ctx.qry.head;
		ctx.qry.head = xstrdup(ctx.repo->defbranch);
		cgit_print_error_page(404, "Not found",
				"Invalid branch: %s", old_head);
		free(old_head);
		return 1;
	}
	string_list_sort(&ctx.repo->submodules);
	cgit_prepare_repo_env(ctx.repo);
	choose_readme(ctx.repo);
	return 0;
}
