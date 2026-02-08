/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"

/* Read the content of the specified file into a newly allocated buffer,
 * zeroterminate the buffer and return 0 on success, errno otherwise.
 */
int readfile(const char *path, char **buf, size_t *size)
{
	int fd, e;
	struct stat st;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return errno;
	if (fstat(fd, &st)) {
		e = errno;
		close(fd);
		return e;
	}
	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return EISDIR;
	}
	*buf = xmalloc(st.st_size + 1);
	*size = read_in_full(fd, *buf, st.st_size);
	e = errno;
	(*buf)[*size] = '\0';
	close(fd);
	return (*size == st.st_size ? 0 : e);
}

static int is_token_char(char c)
{
	return isalnum(c) || c == '_';
}

/* Replace name with getenv(name), return pointer to zero-terminating char
 */
static char *expand_macro(char *name, int maxlength)
{
	char *value;
	size_t len;

	len = 0;
	value = getenv(name);
	if (value) {
		len = strlen(value) + 1;
		if (len > maxlength)
			len = maxlength;
		strlcpy(name, value, len);
		--len;
	}
	return name + len;
}

#define EXPBUFSIZE (1024 * 8)

/* Replace all tokens prefixed by '$' in the specified text with the
 * value of the named environment variable.
 * NB: the return value is a static buffer, i.e. it must be strdup'd
 * by the caller.
 */
char *expand_macros(const char *txt)
{
	static char result[EXPBUFSIZE];
	char *p, *start;
	int len;

	p = result;
	start = NULL;
	while (p < result + EXPBUFSIZE - 1 && txt && *txt) {
		*p = *txt;
		if (start) {
			if (!is_token_char(*txt)) {
				if (p - start > 0) {
					*p = '\0';
					len = result + EXPBUFSIZE - start - 1;
					p = expand_macro(start, len) - 1;
				}
				start = NULL;
				txt--;
			}
			p++;
			txt++;
			continue;
		}
		if (*txt == '$') {
			start = p;
			txt++;
			continue;
		}
		p++;
		txt++;
	}
	*p = '\0';
	if (start && p - start > 0) {
		len = result + EXPBUFSIZE - start - 1;
		p = expand_macro(start, len);
		*p = '\0';
	}
	return result;
}

char *get_mimetype_for_filename(const char *filename)
{
	char *ext, *mimetype, line[1024];
	struct string_list list = STRING_LIST_INIT_NODUP;
	int i;
	FILE *file;
	struct string_list_item *mime;

	if (!filename)
		return NULL;

	ext = strrchr(filename, '.');
	if (!ext)
		return NULL;
	++ext;
	if (!ext[0])
		return NULL;
	mime = string_list_lookup(&ctx.cfg.mimetypes, ext);
	if (mime)
		return xstrdup(mime->util);

	if (!ctx.cfg.mimetype_file)
		return NULL;
	file = fopen(ctx.cfg.mimetype_file, "r");
	if (!file)
		return NULL;
	while (fgets(line, sizeof(line), file)) {
		if (!line[0] || line[0] == '#')
			continue;
		string_list_split_in_place(&list, line, " \t\r\n", -1);
		string_list_remove_empty_items(&list, 0);
		mimetype = list.items[0].string;
		for (i = 1; i < list.nr; i++) {
			if (!strcasecmp(ext, list.items[i].string)) {
				fclose(file);
				return xstrdup(mimetype);
			}
		}
		string_list_clear(&list, 0);
	}
	fclose(file);
	return NULL;
}
