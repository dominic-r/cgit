#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"

const char *cgit_httpscheme(void)
{
	if (ctx.env.https && !strcmp(ctx.env.https, "on"))
		return "https://";
	else
		return "http://";
}

char *cgit_hosturl(void)
{
	if (ctx.env.http_host)
		return xstrdup(ctx.env.http_host);
	if (!ctx.env.server_name)
		return NULL;
	if (!ctx.env.server_port || atoi(ctx.env.server_port) == 80)
		return xstrdup(ctx.env.server_name);
	return fmtalloc("%s:%s", ctx.env.server_name, ctx.env.server_port);
}

char *cgit_currenturl(void)
{
	const char *root = cgit_rooturl();

	if (!ctx.qry.url)
		return xstrdup(root);
	if (root[0] && root[strlen(root) - 1] == '/')
		return fmtalloc("%s%s", root, ctx.qry.url);
	return fmtalloc("%s/%s", root, ctx.qry.url);
}

char *cgit_currentfullurl(void)
{
	const char *root = cgit_rooturl();
	const char *orig_query = ctx.env.query_string ? ctx.env.query_string : "";
	size_t len = strlen(orig_query);
	char *query = xmalloc(len + 2), *start_url, *ret;

	/* Remove all url=... parts from query string */
	memcpy(query + 1, orig_query, len + 1);
	query[0] = '?';
	start_url = query;
	while ((start_url = strstr(start_url, "url=")) != NULL) {
		if (start_url[-1] == '?' || start_url[-1] == '&') {
			const char *end_url = strchr(start_url, '&');
			if (end_url)
				memmove(start_url, end_url + 1, strlen(end_url));
			else
				start_url[0] = '\0';
		} else
			++start_url;
	}
	if (!query[1])
		query[0] = '\0';

	if (!ctx.qry.url)
		ret = fmtalloc("%s%s", root, query);
	else if (root[0] && root[strlen(root) - 1] == '/')
		ret = fmtalloc("%s%s%s", root, ctx.qry.url, query);
	else
		ret = fmtalloc("%s/%s%s", root, ctx.qry.url, query);
	free(query);
	return ret;
}

const char *cgit_rooturl(void)
{
	if (ctx.cfg.virtual_root)
		return ctx.cfg.virtual_root;
	else
		return ctx.cfg.script_name;
}

const char *cgit_loginurl(void)
{
	static const char *login_url;
	if (!login_url)
		login_url = fmtalloc("%s?p=login", cgit_rooturl());
	return login_url;
}

char *cgit_repourl(const char *reponame)
{
	if (ctx.cfg.virtual_root)
		return fmtalloc("%s%s/", ctx.cfg.virtual_root, reponame);
	else
		return fmtalloc("?r=%s", reponame);
}

char *cgit_fileurl(const char *reponame, const char *pagename,
		   const char *filename, const char *query)
{
	struct strbuf sb = STRBUF_INIT;
	char *delim;

	if (ctx.cfg.virtual_root) {
		strbuf_addf(&sb, "%s%s/%s/%s", ctx.cfg.virtual_root, reponame,
			    pagename, (filename ? filename:""));
		delim = "?";
	} else {
		strbuf_addf(&sb, "?url=%s/%s/%s", reponame, pagename,
			    (filename ? filename : ""));
		delim = "&amp;";
	}
	if (query)
		strbuf_addf(&sb, "%s%s", delim, query);
	return strbuf_detach(&sb, NULL);
}

char *cgit_pageurl(const char *reponame, const char *pagename,
		   const char *query)
{
	return cgit_fileurl(reponame, pagename, NULL, query);
}

const char *cgit_repobasename(const char *reponame)
{
	/* I assume we don't need to store more than one repo basename */
	static char rvbuf[1024];
	int p;
	const char *rv;
	size_t len;

	len = strlcpy(rvbuf, reponame, sizeof(rvbuf));
	if (len >= sizeof(rvbuf))
		die("cgit_repobasename: truncated repository name '%s'", reponame);
	p = len - 1;
	/* strip trailing slashes */
	while (p && rvbuf[p] == '/')
		rvbuf[p--] = '\0';
	/* strip trailing .git */
	if (p >= 3 && starts_with(&rvbuf[p-3], ".git")) {
		p -= 3;
		rvbuf[p--] = '\0';
	}
	/* strip more trailing slashes if any */
	while (p && rvbuf[p] == '/')
		rvbuf[p--] = '\0';
	/* find last slash in the remaining string */
	rv = strrchr(rvbuf, '/');
	if (rv)
		return ++rv;
	return rvbuf;
}

const char *cgit_snapshot_prefix(const struct cgit_repo *repo)
{
	if (repo->snapshot_prefix)
		return repo->snapshot_prefix;

	return cgit_repobasename(repo->url);
}
