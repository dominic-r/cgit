/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "html.h"
#include "ui-shared.h"
#include "ui-shared-links.h"

void cgit_object_link(struct object *obj)
{
	char *page, *shortrev, *fullrev, *name;

	fullrev = oid_to_hex(&obj->oid);
	shortrev = xstrdup(fullrev);
	shortrev[10] = '\0';
	if (obj->type == OBJ_COMMIT) {
		cgit_commit_link(fmt("commit %s...", shortrev), NULL, NULL,
				 ctx.qry.head, fullrev, NULL);
		return;
	} else if (obj->type == OBJ_TREE)
		page = "tree";
	else if (obj->type == OBJ_TAG)
		page = "tag";
	else
		page = "blob";
	name = fmt("%s %s...", type_name(obj->type), shortrev);
	cgit_reporevlink(page, name, NULL, NULL, ctx.qry.head, fullrev, NULL);
}

static struct string_list_item *lookup_path(struct string_list *list,
					    const char *path)
{
	struct string_list_item *item;

	while (path && path[0]) {
		if ((item = string_list_lookup(list, path)))
			return item;
		if (!(path = strchr(path, '/')))
			break;
		path++;
	}
	return NULL;
}

void cgit_submodule_link(const char *class, char *path, const char *rev)
{
	struct string_list *list;
	struct string_list_item *item;
	char tail, *dir;
	size_t len;

	len = 0;
	tail = 0;
	list = &ctx.repo->submodules;
	item = lookup_path(list, path);
	if (!item) {
		len = strlen(path);
		tail = path[len - 1];
		if (tail == '/') {
			path[len - 1] = 0;
			item = lookup_path(list, path);
		}
	}
	if (item || ctx.repo->module_link) {
		html("<a ");
		if (class)
			htmlf("class='%s' ", class);
		html("href='");
		if (item) {
			html_attrf(item->util, rev);
		} else {
			dir = strrchr(path, '/');
			if (dir)
				dir++;
			else
				dir = path;
			html_attrf(ctx.repo->module_link, dir, rev);
		}
		html("'>");
		html_txt(path);
		html("</a>");
	} else {
		html("<span");
		if (class)
			htmlf(" class='%s'", class);
		html(">");
		html_txt(path);
		html("</span>");
	}
	html_txtf(" @ %.7s", rev);
	if (item && tail)
		path[len - 1] = tail;
}
