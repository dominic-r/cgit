#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"
#include "cgit-main.h"

static void open_auth_filter(const char *function)
{
	cgit_open_filter(ctx.cfg.auth_filter, function,
		ctx.env.http_cookie ? ctx.env.http_cookie : "",
		ctx.env.request_method ? ctx.env.request_method : "",
		ctx.env.query_string ? ctx.env.query_string : "",
		ctx.env.http_referer ? ctx.env.http_referer : "",
		ctx.env.path_info ? ctx.env.path_info : "",
		ctx.env.http_host ? ctx.env.http_host : "",
		ctx.env.https ? ctx.env.https : "",
		ctx.qry.repo ? ctx.qry.repo : "",
		ctx.qry.page ? ctx.qry.page : "",
		cgit_currentfullurl(),
		cgit_loginurl());
}

/* We intentionally keep this rather small, instead of looping and
 * feeding it to the filter a couple bytes at a time. This way, the
 * filter itself does not need to handle any denial of service or
 * buffer bloat issues. If this winds up being too small, people
 * will complain on the mailing list, and we'll increase it as needed. */
#define MAX_AUTHENTICATION_POST_BYTES 4096
/* The filter is expected to spit out "Status: " and all headers. */
static void authenticate_post(void)
{
	char buffer[MAX_AUTHENTICATION_POST_BYTES];
	ssize_t len;

	open_auth_filter("authenticate-post");
	len = ctx.env.content_length;
	if (len > MAX_AUTHENTICATION_POST_BYTES)
		len = MAX_AUTHENTICATION_POST_BYTES;
	if ((len = read(STDIN_FILENO, buffer, len)) < 0)
		die_errno("Could not read POST from stdin");
	if (write(STDOUT_FILENO, buffer, len) < 0)
		die_errno("Could not write POST to stdout");
	cgit_close_filter(ctx.cfg.auth_filter);
	exit(0);
}

void cgit_authenticate_cookie(void)
{
	/* If we don't have an auth_filter, consider all cookies valid, and thus return early. */
	if (!ctx.cfg.auth_filter) {
		ctx.env.authenticated = 1;
		return;
	}

	/* If we're having something POST'd to /login, we're authenticating POST,
	 * instead of the cookie, so call authenticate_post and bail out early.
	 * This pattern here should match /?p=login with POST. */
	if (ctx.env.request_method && ctx.qry.page && !ctx.repo &&
	    !strcmp(ctx.env.request_method, "POST") && !strcmp(ctx.qry.page, "login")) {
		authenticate_post();
		return;
	}

	/* If we've made it this far, we're authenticating the cookie for real, so do that. */
	open_auth_filter("authenticate-cookie");
	ctx.env.authenticated = cgit_close_filter(ctx.cfg.auth_filter);
}

void cgit_auth_print_body(void)
{
	open_auth_filter("body");
	cgit_close_filter(ctx.cfg.auth_filter);
}
