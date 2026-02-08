#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"
#include "html.h"

/* macOS compat: memrchr is a GNU extension */
#ifndef HAVE_MEMRCHR
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
static inline void *memrchr(const void *s, int c, size_t n)
{
	const unsigned char *p = (const unsigned char *)s + n;
	while (n--) {
		if (*--p == (unsigned char)c)
			return (void *)p;
	}
	return NULL;
}
#endif
#endif

void cgit_print_filemode(unsigned short mode)
{
	if (S_ISDIR(mode))
		html("d");
	else if (S_ISLNK(mode))
		html("l");
	else if (S_ISGITLINK(mode))
		html("m");
	else
		html("-");
	html_fileperm(mode >> 6);
	html_fileperm(mode >> 3);
	html_fileperm(mode);
}

void cgit_compose_snapshot_prefix(struct strbuf *filename, const char *base,
				  const char *ref)
{
	struct object_id oid;

	/*
	 * Prettify snapshot names by stripping leading "v" or "V" if the tag
	 * name starts with {v,V}[0-9] and the prettify mapping is injective,
	 * i.e. each stripped tag can be inverted without ambiguities.
	 */
	if (repo_get_oid(the_repository, fmt("refs/tags/%s", ref), &oid) == 0 &&
	    (ref[0] == 'v' || ref[0] == 'V') && isdigit(ref[1]) &&
	    ((repo_get_oid(the_repository, fmt("refs/tags/%s", ref + 1), &oid) == 0) +
	     (repo_get_oid(the_repository, fmt("refs/tags/v%s", ref + 1), &oid) == 0) +
	     (repo_get_oid(the_repository, fmt("refs/tags/V%s", ref + 1), &oid) == 0) == 1))
		ref++;

	strbuf_addf(filename, "%s-%s", base, ref);
}

void cgit_print_snapshot_links(const struct cgit_repo *repo, const char *ref,
			       const char *separator)
{
	const struct cgit_snapshot_format *f;
	struct strbuf filename = STRBUF_INIT;
	const char *basename;
	size_t prefixlen;

	basename = cgit_snapshot_prefix(repo);
	if (starts_with(ref, basename))
		strbuf_addstr(&filename, ref);
	else
		cgit_compose_snapshot_prefix(&filename, basename, ref);

	prefixlen = filename.len;
	for (f = cgit_snapshot_formats; f->suffix; f++) {
		if (!(repo->snapshots & cgit_snapshot_format_bit(f)))
			continue;
		strbuf_setlen(&filename, prefixlen);
		strbuf_addstr(&filename, f->suffix);
		cgit_snapshot_link(filename.buf, NULL, NULL, NULL, NULL,
				   filename.buf);
		if (cgit_snapshot_get_sig(ref, f)) {
			strbuf_addstr(&filename, ".asc");
			html(" (");
			cgit_snapshot_link("sig", NULL, NULL, NULL, NULL,
					   filename.buf);
			html(")");
		} else if (starts_with(f->suffix, ".tar") && cgit_snapshot_get_sig(ref, &cgit_snapshot_formats[0])) {
			strbuf_setlen(&filename, strlen(filename.buf) - strlen(f->suffix));
			strbuf_addstr(&filename, ".tar.asc");
			html(" (");
			cgit_snapshot_link("sig", NULL, NULL, NULL, NULL,
					   filename.buf);
			html(")");
		}
		html(separator);
	}
	strbuf_release(&filename);
}

void cgit_set_title_from_path(const char *path)
{
	struct strbuf sb = STRBUF_INIT;
	const char *slash, *last_slash;

	if (!path)
		return;

	for (last_slash = path + strlen(path); (slash = memrchr(path, '/', last_slash - path)) != NULL; last_slash = slash) {
		strbuf_add(&sb, slash + 1, last_slash - slash - 1);
		strbuf_addstr(&sb, " \xc2\xab ");
	}
	strbuf_add(&sb, path, last_slash - path);
	strbuf_addf(&sb, " - %s", ctx.page.title);
	ctx.page.title = strbuf_detach(&sb, NULL);
}
