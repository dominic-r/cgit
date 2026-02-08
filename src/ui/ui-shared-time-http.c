/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"
#include "html.h"

static char *http_date(time_t t)
{
	static char day[][4] =
		{"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	static char month[][4] =
		{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	struct tm tm;
	gmtime_r(&t, &tm);
	return fmt("%s, %02d %s %04d %02d:%02d:%02d GMT", day[tm.tm_wday],
		   tm.tm_mday, month[tm.tm_mon], 1900 + tm.tm_year,
		   tm.tm_hour, tm.tm_min, tm.tm_sec);
}

const struct date_mode cgit_date_mode(enum date_mode_type type)
{
	static struct date_mode mode;
	mode.type = type;
	mode.local = ctx.cfg.local_time;
	return mode;
}

static void print_rel_date(time_t t, int tz, double value,
	const char *class, const char *suffix)
{
	htmlf("<span class='%s' data-ut='%" PRIu64 "' title='", class, (uint64_t)t);
	html_attr(show_date(t, tz, cgit_date_mode(DATE_ISO8601)));
	htmlf("'>%.0f %s</span>", value, suffix);
}

void cgit_print_age(time_t t, int tz, time_t max_relative)
{
	time_t now, secs;

	if (!t)
		return;
	time(&now);
	secs = now - t;
	if (secs < 0)
		secs = 0;

	if (secs > max_relative && max_relative >= 0) {
		html("<span title='");
		html_attr(show_date(t, tz, cgit_date_mode(DATE_ISO8601)));
		html("'>");
		html_txt(show_date(t, tz, cgit_date_mode(DATE_SHORT)));
		html("</span>");
		return;
	}

	if (secs < TM_HOUR * 2) {
		print_rel_date(t, tz, secs * 1.0 / TM_MIN, "age-mins", "min.");
		return;
	}
	if (secs < TM_DAY * 2) {
		print_rel_date(t, tz, secs * 1.0 / TM_HOUR, "age-hours", "hours");
		return;
	}
	if (secs < TM_WEEK * 2) {
		print_rel_date(t, tz, secs * 1.0 / TM_DAY, "age-days", "days");
		return;
	}
	if (secs < TM_MONTH * 2) {
		print_rel_date(t, tz, secs * 1.0 / TM_WEEK, "age-weeks", "weeks");
		return;
	}
	if (secs < TM_YEAR * 2) {
		print_rel_date(t, tz, secs * 1.0 / TM_MONTH, "age-months", "months");
		return;
	}
	print_rel_date(t, tz, secs * 1.0 / TM_YEAR, "age-years", "years");
}

void cgit_print_http_headers(void)
{
	if (ctx.env.no_http && !strcmp(ctx.env.no_http, "1"))
		return;

	if (ctx.page.status)
		htmlf("Status: %d %s\n", ctx.page.status, ctx.page.statusmsg);
	if (ctx.page.mimetype && ctx.page.charset)
		htmlf("Content-Type: %s; charset=%s\n", ctx.page.mimetype,
		      ctx.page.charset);
	else if (ctx.page.mimetype)
		htmlf("Content-Type: %s\n", ctx.page.mimetype);
	if (ctx.page.size)
		htmlf("Content-Length: %zd\n", ctx.page.size);
	if (ctx.page.filename) {
		html("Content-Disposition: inline; filename=\"");
		html_header_arg_in_quotes(ctx.page.filename);
		html("\"\n");
	}
	if (!ctx.env.authenticated)
		html("Cache-Control: no-cache, no-store\n");
	htmlf("Last-Modified: %s\n", http_date(ctx.page.modified));
	htmlf("Expires: %s\n", http_date(ctx.page.expires));
	if (ctx.page.etag)
		htmlf("ETag: \"%s\"\n", ctx.page.etag);
	html("\n");
	if (ctx.env.request_method && !strcmp(ctx.env.request_method, "HEAD"))
		exit(0);
}

void cgit_redirect(const char *url, bool permanent)
{
	htmlf("Status: %d %s\n", permanent ? 301 : 302, permanent ? "Moved" : "Found");
	html("Location: ");
	html_url_path(url);
	html("\n\n");
}
