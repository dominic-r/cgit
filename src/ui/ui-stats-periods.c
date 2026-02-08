/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-stats.h"

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

const struct cgit_period cgit_periods[] = {
	{'d', "day", 12, 7, trunc_day, dec_day, inc_day, pretty_day},
	{'w', "week", 12, 4, trunc_week, dec_week, inc_week, pretty_week},
	{'m', "month", 12, 4, trunc_month, dec_month, inc_month, pretty_month},
	{'y', "year", 64, 4, trunc_year, dec_year, inc_year, pretty_year},
	{'q', "quarter", 12, 4, trunc_quarter, dec_quarter, inc_quarter, pretty_quarter},
};

const size_t cgit_periods_count = ARRAY_SIZE(cgit_periods);

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

	for (i = 0; i < cgit_periods_count; i++)
		if (cgit_periods[i].code == code || !strcmp(cgit_periods[i].name, expr)) {
			if (period)
				*period = &cgit_periods[i];
			return i + 1;
		}
	return 0;
}

const char *cgit_find_stats_periodname(int idx)
{
	if (idx > 0 && idx <= cgit_periods_count)
		return cgit_periods[idx - 1].name;
	else
		return "";
}
