/* shared.c: global vars + some callback functions
 *
 * Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"

struct cgit_repolist cgit_repolist;
struct cgit_context ctx;

int chk_zero(int result, char *msg)
{
	if (result != 0)
		die_errno("%s", msg);
	return result;
}

int chk_positive(int result, char *msg)
{
	if (result <= 0)
		die_errno("%s", msg);
	return result;
}

int chk_non_negative(int result, char *msg)
{
	if (result < 0)
		die_errno("%s", msg);
	return result;
}

char *cgit_default_repo_desc = "[no description]";
struct cgit_repo *cgit_add_repo(const char *url)
{
	struct cgit_repo *ret;

	if (++cgit_repolist.count > cgit_repolist.length) {
		if (cgit_repolist.length == 0)
			cgit_repolist.length = 8;
		else
			cgit_repolist.length *= 2;
		cgit_repolist.repos = xrealloc(cgit_repolist.repos,
					       cgit_repolist.length *
					       sizeof(struct cgit_repo));
	}

	ret = &cgit_repolist.repos[cgit_repolist.count-1];
	memset(ret, 0, sizeof(struct cgit_repo));
	ret->url = trim_end(url, '/');
	ret->name = ret->url;
	ret->path = NULL;
	ret->desc = cgit_default_repo_desc;
	ret->extra_head_content = NULL;
	ret->owner = NULL;
	ret->homepage = NULL;
	ret->section = ctx.cfg.section;
	ret->snapshots = ctx.cfg.snapshots;
	ret->enable_blame = ctx.cfg.enable_blame;
	ret->enable_commit_graph = ctx.cfg.enable_commit_graph;
	ret->enable_log_filecount = ctx.cfg.enable_log_filecount;
	ret->enable_log_linecount = ctx.cfg.enable_log_linecount;
	ret->enable_remote_branches = ctx.cfg.enable_remote_branches;
	ret->enable_subject_links = ctx.cfg.enable_subject_links;
	ret->enable_html_serving = ctx.cfg.enable_html_serving;
	ret->max_stats = ctx.cfg.max_stats;
	ret->branch_sort = ctx.cfg.branch_sort;
	ret->commit_sort = ctx.cfg.commit_sort;
	ret->module_link = ctx.cfg.module_link;
	ret->readme = ctx.cfg.readme;
	ret->mtime = -1;
	ret->about_filter = ctx.cfg.about_filter;
	ret->commit_filter = ctx.cfg.commit_filter;
	ret->source_filter = ctx.cfg.source_filter;
	ret->email_filter = ctx.cfg.email_filter;
	ret->owner_filter = ctx.cfg.owner_filter;
	ret->clone_url = ctx.cfg.clone_url;
	ret->submodules.strdup_strings = 1;
	ret->hide = ret->ignore = 0;
	return ret;
}

struct cgit_repo *cgit_get_repoinfo(const char *url)
{
	int i;
	size_t len;
	struct cgit_repo *repo;
	char *alt_url = NULL;

	len = strlen(url);

	/* Try exact match first */
	for (i = 0; i < cgit_repolist.count; i++) {
		repo = &cgit_repolist.repos[i];
		if (repo->ignore)
			continue;
		if (!strcmp(repo->url, url))
			return repo;
	}

	/* Try alternate form: with or without .git suffix */
	if (len > 4 && !strcmp(url + len - 4, ".git")) {
		/* URL has .git, try without */
		alt_url = xstrdup(url);
		alt_url[len - 4] = '\0';
	} else {
		/* URL has no .git, try with */
		alt_url = xstrfmt("%s.git", url);
	}

	for (i = 0; i < cgit_repolist.count; i++) {
		repo = &cgit_repolist.repos[i];
		if (repo->ignore)
			continue;
		if (!strcmp(repo->url, alt_url)) {
			free(alt_url);
			return repo;
		}
	}

	free(alt_url);
	return NULL;
}

void cgit_free_commitinfo(struct commitinfo *info)
{
	free(info->author);
	free(info->author_email);
	free(info->committer);
	free(info->committer_email);
	free(info->subject);
	free(info->msg);
	free(info->msg_encoding);
	free(info);
}

char *trim_end(const char *str, char c)
{
	int len;

	if (str == NULL)
		return NULL;
	len = strlen(str);
	while (len > 0 && str[len - 1] == c)
		len--;
	if (len == 0)
		return NULL;
	return xstrndup(str, len);
}

char *ensure_end(const char *str, char c)
{
	size_t len = strlen(str);
	char *result;

	if (len && str[len - 1] == c)
		return xstrndup(str, len);

	result = xmalloc(len + 2);
	memcpy(result, str, len);
	result[len] = '/';
	result[len + 1] = '\0';
	return result;
}

void strbuf_ensure_end(struct strbuf *sb, char c)
{
	if (!sb->len || sb->buf[sb->len - 1] != c)
		strbuf_addch(sb, c);
}

void cgit_add_ref(struct reflist *list, struct refinfo *ref)
{
	size_t size;

	if (list->count >= list->alloc) {
		list->alloc += (list->alloc ? list->alloc : 4);
		size = list->alloc * sizeof(struct refinfo *);
		list->refs = xrealloc(list->refs, size);
	}
	list->refs[list->count++] = ref;
}

static struct refinfo *cgit_mk_refinfo(const char *refname, const struct object_id *oid)
{
	struct refinfo *ref;

	ref = xmalloc(sizeof (struct refinfo));
	ref->refname = xstrdup(refname);
	ref->object = parse_object(the_repository, oid);
	switch (ref->object->type) {
	case OBJ_TAG:
		ref->tag = cgit_parse_tag((struct tag *)ref->object);
		break;
	case OBJ_COMMIT:
		ref->commit = cgit_parse_commit((struct commit *)ref->object);
		break;
	}
	return ref;
}

void cgit_free_taginfo(struct taginfo *tag)
{
	if (tag->tagger)
		free(tag->tagger);
	if (tag->tagger_email)
		free(tag->tagger_email);
	if (tag->msg)
		free(tag->msg);
	free(tag);
}

static void cgit_free_refinfo(struct refinfo *ref)
{
	if (ref->refname)
		free((char *)ref->refname);
	switch (ref->object->type) {
	case OBJ_TAG:
		cgit_free_taginfo(ref->tag);
		break;
	case OBJ_COMMIT:
		cgit_free_commitinfo(ref->commit);
		break;
	}
	free(ref);
}

void cgit_free_reflist_inner(struct reflist *list)
{
	int i;

	for (i = 0; i < list->count; i++) {
		cgit_free_refinfo(list->refs[i]);
	}
	free(list->refs);
}

int cgit_refs_cb(const struct reference *ref, void *cb_data)
{
	struct reflist *list = (struct reflist *)cb_data;
	struct refinfo *info = cgit_mk_refinfo(ref->name, ref->oid);

	if (info)
		cgit_add_ref(list, info);
	return 0;
}

int cgit_parse_snapshots_mask(const char *str)
{
	struct string_list tokens = STRING_LIST_INIT_DUP;
	struct string_list_item *item;
	const struct cgit_snapshot_format *f;
	int rv = 0;

	/* favor legacy setting */
	if (atoi(str))
		return 1;

	if (strcmp(str, "all") == 0)
		return INT_MAX;

	string_list_split(&tokens, str, " ", -1);
	string_list_remove_empty_items(&tokens, 0);

	for_each_string_list_item(item, &tokens) {
		for (f = cgit_snapshot_formats; f->suffix; f++) {
			if (!strcmp(item->string, f->suffix) ||
			    !strcmp(item->string, f->suffix + 1)) {
				rv |= cgit_snapshot_format_bit(f);
				break;
			}
		}
	}

	string_list_clear(&tokens, 0);
	return rv;
}

typedef struct {
	char * name;
	char * value;
} cgit_env_var;

void cgit_prepare_repo_env(struct cgit_repo * repo)
{
	cgit_env_var env_vars[] = {
		{ .name = "CGIT_REPO_URL", .value = repo->url },
		{ .name = "CGIT_REPO_NAME", .value = repo->name },
		{ .name = "CGIT_REPO_PATH", .value = repo->path },
		{ .name = "CGIT_REPO_OWNER", .value = repo->owner },
		{ .name = "CGIT_REPO_DEFBRANCH", .value = repo->defbranch },
		{ .name = "CGIT_REPO_SECTION", .value = repo->section },
		{ .name = "CGIT_REPO_CLONE_URL", .value = repo->clone_url }
	};
	int env_var_count = ARRAY_SIZE(env_vars);
	cgit_env_var *p, *q;
	static char *warn = "cgit warning: failed to set env: %s=%s\n";

	p = env_vars;
	q = p + env_var_count;
	for (; p < q; p++)
		if (p->value && setenv(p->name, p->value, 1))
			fprintf(stderr, warn, p->name, p->value);
}
