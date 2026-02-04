/* ui-stats.c: generate stats view
 *
 * Copyright (C) 2006-2014 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-stats.h"
#include "html.h"
#include "ui-shared.h"

struct authorstat {
	long total;
	struct string_list list;
};

#define DAY_SECS (60 * 60 * 24)
#define WEEK_SECS (DAY_SECS * 7)

static void trunc_day(struct tm *tm UNUSED)
{
}

static void dec_day(struct tm *tm)
{
	time_t t = timegm(tm);
	t -= DAY_SECS;
	gmtime_r(&t, tm);
}

static void inc_day(struct tm *tm)
{
	time_t t = timegm(tm);
	t += DAY_SECS;
	gmtime_r(&t, tm);
}

static char *pretty_day(struct tm *tm)
{
	static char buf[16];

	strftime(buf, sizeof(buf), "%d %b", tm);
	return buf;
}

static void trunc_week(struct tm *tm)
{
	time_t t = timegm(tm);
	t -= ((tm->tm_wday + 6) % 7) * DAY_SECS;
	gmtime_r(&t, tm);
}

static void dec_week(struct tm *tm)
{
	time_t t = timegm(tm);
	t -= WEEK_SECS;
	gmtime_r(&t, tm);
}

static void inc_week(struct tm *tm)
{
	time_t t = timegm(tm);
	t += WEEK_SECS;
	gmtime_r(&t, tm);
}

static char *pretty_week(struct tm *tm)
{
	static char buf[10];

	strftime(buf, sizeof(buf), "W%V %G", tm);
	return buf;
}

static void trunc_month(struct tm *tm)
{
	tm->tm_mday = 1;
}

static void dec_month(struct tm *tm)
{
	tm->tm_mon--;
	if (tm->tm_mon < 0) {
		tm->tm_year--;
		tm->tm_mon = 11;
	}
}

static void inc_month(struct tm *tm)
{
	tm->tm_mon++;
	if (tm->tm_mon > 11) {
		tm->tm_year++;
		tm->tm_mon = 0;
	}
}

static char *pretty_month(struct tm *tm)
{
	static const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	return fmt("%s %d", months[tm->tm_mon], tm->tm_year + 1900);
}

static void trunc_quarter(struct tm *tm)
{
	trunc_month(tm);
	while (tm->tm_mon % 3 != 0)
		dec_month(tm);
}

static void dec_quarter(struct tm *tm)
{
	dec_month(tm);
	dec_month(tm);
	dec_month(tm);
}

static void inc_quarter(struct tm *tm)
{
	inc_month(tm);
	inc_month(tm);
	inc_month(tm);
}

static char *pretty_quarter(struct tm *tm)
{
	return fmt("Q%d %d", tm->tm_mon / 3 + 1, tm->tm_year + 1900);
}

static void trunc_year(struct tm *tm)
{
	trunc_month(tm);
	tm->tm_mon = 0;
}

static void dec_year(struct tm *tm)
{
	tm->tm_year--;
}

static void inc_year(struct tm *tm)
{
	tm->tm_year++;
}

static char *pretty_year(struct tm *tm)
{
	return fmt("%d", tm->tm_year + 1900);
}

static const struct cgit_period periods[] = {
	{'d', "day", 12, 7, trunc_day, dec_day, inc_day, pretty_day},
	{'w', "week", 12, 4, trunc_week, dec_week, inc_week, pretty_week},
	{'m', "month", 12, 4, trunc_month, dec_month, inc_month, pretty_month},
	{'y', "year", 64, 4, trunc_year, dec_year, inc_year, pretty_year},
	{'q', "quarter", 12, 4, trunc_quarter, dec_quarter, inc_quarter, pretty_quarter},
};

/* Given a period code or name, return a period index (1..ARRAY_SIZE(periods))
 * and update the period pointer to the correcsponding struct.
 * If no matching code is found, return 0.
 */
int cgit_find_stats_period(const char *expr, const struct cgit_period **period)
{
	int i;
	char code = '\0';

	if (!expr)
		return 0;

	if (strlen(expr) == 1)
		code = expr[0];

	for (i = 0; i < sizeof(periods) / sizeof(periods[0]); i++)
		if (periods[i].code == code || !strcmp(periods[i].name, expr)) {
			if (period)
				*period = &periods[i];
			return i + 1;
		}
	return 0;
}

const char *cgit_find_stats_periodname(int idx)
{
	if (idx > 0 && idx <= ARRAY_SIZE(periods))
		return periods[idx - 1].name;
	else
		return "";
}

static void add_author_commit(struct string_list *authors, const char *author_name,
			      unsigned long committer_date,
			      const struct cgit_period *period)
{
	struct string_list_item *author_item, *item;
	struct authorstat *authorstat;
	struct string_list *items;
	char *tmp;
	struct tm date;
	time_t t;
	uintptr_t *counter;

	tmp = xstrdup(author_name ? author_name : "");
	author_item = string_list_insert(authors, tmp);
	if (!author_item->util)
		author_item->util = xcalloc(1, sizeof(struct authorstat));
	else
		free(tmp);
	authorstat = author_item->util;
	items = &authorstat->list;
	t = committer_date;
	gmtime_r(&t, &date);
	period->trunc(&date);
	tmp = xstrdup(period->pretty(&date));
	item = string_list_insert(items, tmp);
	counter = (uintptr_t *)&item->util;
	if (*counter)
		free(tmp);
	(*counter)++;

	authorstat->total++;
}

static void add_commit(struct string_list *authors, struct commit *commit,
	const struct cgit_period *period)
{
	struct commitinfo *info;

	info = cgit_parse_commit(commit);
	add_author_commit(authors, info->author, info->committer_date, period);
	cgit_free_commitinfo(info);
}

static int cmp_total_commits(const void *a1, const void *a2)
{
	const struct string_list_item *i1 = a1;
	const struct string_list_item *i2 = a2;
	const struct authorstat *auth1 = i1->util;
	const struct authorstat *auth2 = i2->util;

	return auth2->total - auth1->total;
}

/* Walk the commit DAG and collect number of commits per author per
 * timeperiod into a nested string_list collection.
 */
static struct string_list collect_stats(const struct cgit_period *period)
{
	struct string_list authors;
	struct rev_info rev;
	struct commit *commit;
	struct strvec rev_argv = STRVEC_INIT;
	time_t now;
	long i;
	struct tm tm;
	char tmp[11];

	time(&now);
	gmtime_r(&now, &tm);
	period->trunc(&tm);
	for (i = 1; i < period->count; i++)
		period->dec(&tm);
	strftime(tmp, sizeof(tmp), "%Y-%m-%d", &tm);
	strvec_push(&rev_argv, "stats_rev_setup");
	strvec_push(&rev_argv, ctx.qry.head ? ctx.qry.head : "HEAD");
	strvec_pushf(&rev_argv, "--since=%s", tmp);
	if (ctx.qry.path) {
		strvec_push(&rev_argv, "--");
		strvec_push(&rev_argv, ctx.qry.path);
	}
	repo_init_revisions(the_repository, &rev, NULL);
	rev.ignore_missing = 1;
	setup_revisions(rev_argv.nr, rev_argv.v, &rev, NULL);
	rev.simplify_history = 0;
	if (prepare_revision_walk(&rev)) {
		strvec_clear(&rev_argv);
		release_revisions(&rev);
		clear_object_flags(the_repository, 0xffffffffu);
		memset(&authors, 0, sizeof(authors));
		return authors;
	}
	memset(&authors, 0, sizeof(authors));
	while ((commit = get_revision(&rev)) != NULL) {
		add_commit(&authors, commit, period);
	}
	strvec_clear(&rev_argv);
	release_revisions(&rev);
	clear_object_flags(the_repository, 0xffffffffu);
	return authors;
}

static int get_oldest_commit_time(time_t *oldest)
{
	struct rev_info rev;
	struct commit *commit;
	struct strvec rev_argv = STRVEC_INIT;

	*oldest = 0;
	strvec_push(&rev_argv, "stats_oldest_rev_setup");
	strvec_push(&rev_argv, ctx.qry.head ? ctx.qry.head : "HEAD");
	if (ctx.qry.path) {
		strvec_push(&rev_argv, "--");
		strvec_push(&rev_argv, ctx.qry.path);
	}

	repo_init_revisions(the_repository, &rev, NULL);
	rev.ignore_missing = 1;
	setup_revisions(rev_argv.nr, rev_argv.v, &rev, NULL);
	rev.simplify_history = 0;
	if (prepare_revision_walk(&rev))
		goto done;

	while ((commit = get_revision(&rev)) != NULL) {
		if (*oldest == 0 || *oldest > commit->date)
			*oldest = commit->date;
	}

done:
	strvec_clear(&rev_argv);
	release_revisions(&rev);
	clear_object_flags(the_repository, 0xffffffffu);
	return *oldest != 0;
}

static void print_combined_authorrow(struct string_list *authors, int from,
				     int to, const char *name,
				     const char *leftclass,
				     const char *centerclass,
				     const char *rightclass,
				     const struct cgit_period *period)
{
	struct string_list_item *author;
	struct authorstat *authorstat;
	struct string_list *items;
	struct string_list_item *date;
	time_t now;
	long i, j, total, subtotal;
	struct tm tm;
	char *tmp;

	time(&now);
	gmtime_r(&now, &tm);
	period->trunc(&tm);
	for (i = 1; i < period->count; i++)
		period->dec(&tm);

	total = 0;
	htmlf("<tr><td class='%s'>%s</td>", leftclass,
		fmt(name, to - from + 1));
	for (j = 0; j < period->count; j++) {
		tmp = period->pretty(&tm);
		period->inc(&tm);
		subtotal = 0;
		for (i = from; i <= to; i++) {
			author = &authors->items[i];
			authorstat = author->util;
			items = &authorstat->list;
			date = string_list_lookup(items, tmp);
			if (date)
				subtotal += (uintptr_t)date->util;
		}
		htmlf("<td class='%s'>%ld</td>", centerclass, subtotal);
		total += subtotal;
	}
	htmlf("<td class='%s'>%ld</td></tr>", rightclass, total);
}

static void print_authors(struct string_list *authors, int top,
			  const struct cgit_period *period)
{
	struct string_list_item *author;
	struct authorstat *authorstat;
	struct string_list *items;
	struct string_list_item *date;
	time_t now;
	long i, j, total;
	struct tm tm;
	char *tmp;

	time(&now);
	gmtime_r(&now, &tm);
	period->trunc(&tm);
	for (i = 1; i < period->count; i++)
		period->dec(&tm);

	html("<table class='stats'><tr><th>Author</th>");
	for (j = 0; j < period->count; j++) {
		tmp = period->pretty(&tm);
		htmlf("<th>%s</th>", tmp);
		period->inc(&tm);
	}
	html("<th>Total</th></tr>\n");

	if (top <= 0 || top > authors->nr)
		top = authors->nr;

	for (i = 0; i < top; i++) {
		author = &authors->items[i];
		html("<tr><td class='left'>");
		html_txt(author->string);
		html("</td>");
		authorstat = author->util;
		items = &authorstat->list;
		total = 0;
		for (j = 0; j < period->count; j++)
			period->dec(&tm);
		for (j = 0; j < period->count; j++) {
			tmp = period->pretty(&tm);
			period->inc(&tm);
			date = string_list_lookup(items, tmp);
			if (!date)
				html("<td>0</td>");
			else {
				htmlf("<td>%lu</td>", (uintptr_t)date->util);
				total += (uintptr_t)date->util;
			}
		}
		htmlf("<td class='sum'>%ld</td></tr>", total);
	}

	if (top < authors->nr)
		print_combined_authorrow(authors, top, authors->nr - 1,
			"Others (%ld)", "left", "", "sum", period);

	print_combined_authorrow(authors, 0, authors->nr - 1, "Total",
		"total", "sum", "sum", period);
	html("</table>");
}

struct period_stats {
	const struct cgit_period *period;
	int count;
	struct string_list authors;
	time_t since;
};

static time_t period_start_time(const struct cgit_period *period, int count)
{
	time_t now;
	long i;
	struct tm tm;

	time(&now);
	gmtime_r(&now, &tm);
	period->trunc(&tm);
	for (i = 1; i < count; i++)
		period->dec(&tm);
	tm.tm_hour = 0;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	return timegm(&tm);
}

static void collect_multi_stats(struct period_stats *stats, int nr)
{
	struct rev_info rev;
	struct commit *commit;
	struct strvec rev_argv = STRVEC_INIT;
	struct commitinfo *info;
	int i;
	time_t min_since = 0;
	struct tm min_tm;
	char date_buf[11];

	for (i = 0; i < nr; i++) {
		if (!min_since || stats[i].since < min_since)
			min_since = stats[i].since;
	}

	strvec_push(&rev_argv, "stats_multi_rev_setup");
	strvec_push(&rev_argv, ctx.qry.head ? ctx.qry.head : "HEAD");
	if (min_since) {
		gmtime_r(&min_since, &min_tm);
		strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &min_tm);
		strvec_pushf(&rev_argv, "--since=%s", date_buf);
	}
	if (ctx.qry.path) {
		strvec_push(&rev_argv, "--");
		strvec_push(&rev_argv, ctx.qry.path);
	}

	repo_init_revisions(the_repository, &rev, NULL);
	rev.ignore_missing = 1;
	setup_revisions(rev_argv.nr, rev_argv.v, &rev, NULL);
	rev.simplify_history = 0;
	if (prepare_revision_walk(&rev))
		goto out;

	while ((commit = get_revision(&rev)) != NULL) {
		info = cgit_parse_commit(commit);
		for (i = 0; i < nr; i++) {
			if (info->committer_date >= stats[i].since) {
				add_author_commit(&stats[i].authors, info->author,
						 info->committer_date, stats[i].period);
			}
		}
		cgit_free_commitinfo(info);
	}

out:
	strvec_clear(&rev_argv);
	release_revisions(&rev);
	clear_object_flags(the_repository, 0xffffffffu);
}

static void print_period_stats(struct string_list *authors,
			       const struct cgit_period *period, int count,
			       int top, int include_path)
{
	struct cgit_period effective = *period;

	effective.count = count;
	qsort(authors->items, authors->nr, sizeof(struct string_list_item),
		cmp_total_commits);

	htmlf("<h2>Commits per author per %s", effective.name);
	if (include_path && ctx.qry.path) {
		html(" (path '");
		html_txt(ctx.qry.path);
		html("')");
	}
	html("</h2>");
	print_authors(authors, top, &effective);
}

static void show_period_stats(const struct cgit_period *period, int top, int include_path)
{
	struct string_list authors;
	struct cgit_period effective = *period;

	/*
	 * Show all available years so totals match repository-wide commit counts.
	 */
	if (effective.code == 'y') {
		time_t now, oldest;
		struct tm now_tm, oldest_tm;
		int span;

		if (get_oldest_commit_time(&oldest)) {
			time(&now);
			gmtime_r(&now, &now_tm);
			gmtime_r(&oldest, &oldest_tm);
			span = now_tm.tm_year - oldest_tm.tm_year + 1;
			if (span > 0) {
				if (span > effective.max_periods)
					span = effective.max_periods;
				effective.count = span;
			}
		}
	}

	authors = collect_stats(&effective);
	print_period_stats(&authors, &effective, effective.count, top, include_path);
}

/* Create a sorted string_list with one entry per author. The util-field
 * for each author is another string_list which is used to calculate the
 * number of commits per time-interval.
 */
void cgit_show_stats(void)
{
	const struct cgit_period *period;
	int top, i, max_stats, period_idx = 0;

	max_stats = ctx.repo->max_stats ? ctx.repo->max_stats : ctx.cfg.max_stats;
	if (!max_stats)
		max_stats = cgit_find_stats_period("year", NULL);
	if (ctx.qry.period) {
		period_idx = cgit_find_stats_period(ctx.qry.period, &period);
		if (!period_idx) {
			cgit_print_error_page(404, "Not found",
				"Unknown statistics type: %c", ctx.qry.period[0]);
			return;
		}
		if (period_idx > max_stats) {
			cgit_print_error_page(400, "Bad request",
					"Statistics type disabled: %s", period->name);
			return;
		}
	}

	top = ctx.qry.ofs;
	if (!top)
		top = 10;

	cgit_print_layout_start();
	html("<div class='cgit-panel'>");
	html("<b>stat options</b>");
	html("<form method='get'>");
	cgit_add_hidden_formfields(1, 0, "stats");
	html("<table><tr><td colspan='2'/></tr>");
	if (ctx.qry.period && max_stats > 1) {
		html("<tr><td class='label'>Period:</td>");
		html("<td class='ctrl'><select name='period' onchange='this.form.submit();'>");
		for (i = 0; i < max_stats; i++)
			html_option(fmt("%c", periods[i].code),
				    periods[i].name, fmt("%c", period->code));
		html("</select></td></tr>");
	}
	html("<tr><td class='label'>Authors:</td>");
	html("<td class='ctrl'><select name='ofs' onchange='this.form.submit();'>");
	html_intoption(10, "10", top);
	html_intoption(25, "25", top);
	html_intoption(50, "50", top);
	html_intoption(100, "100", top);
	html_intoption(-1, "all", top);
	html("</select></td></tr>");
	html("<tr><td/><td class='ctrl'>");
	html("<noscript><input type='submit' value='Reload'/></noscript>");
	html("</td></tr></table>");
	html("</form>");
	html("</div>");

	if (ctx.qry.period) {
		show_period_stats(period, top, 1);
	} else {
		int nr = max_stats;
		struct period_stats *stats;

		if (nr > ARRAY_SIZE(periods))
			nr = ARRAY_SIZE(periods);
		stats = xcalloc(nr, sizeof(*stats));
		for (i = 0; i < nr; i++) {
			stats[i].period = &periods[i];
			stats[i].count = stats[i].period->count;
			if (stats[i].period->code == 'y') {
				time_t now, oldest;
				struct tm now_tm, oldest_tm;
				int span;

				if (get_oldest_commit_time(&oldest)) {
					time(&now);
					gmtime_r(&now, &now_tm);
					gmtime_r(&oldest, &oldest_tm);
					span = now_tm.tm_year - oldest_tm.tm_year + 1;
					if (span > 0) {
						if (span > stats[i].period->max_periods)
							span = stats[i].period->max_periods;
						stats[i].count = span;
					}
				}
			}
			stats[i].since = period_start_time(stats[i].period, stats[i].count);
		}
		collect_multi_stats(stats, nr);
		for (i = 0; i < nr; i++) {
			print_period_stats(&stats[i].authors, stats[i].period, stats[i].count, top, i == 0);
			if (i + 1 < nr)
				html("<br/>");
		}
		free(stats);
	}
	cgit_print_layout_end();
}
