#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-stats.h"
#include "html.h"
#include "ui-shared.h"
#include "ui-stats-render.h"

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

void cgit_print_authors_table(struct string_list *authors, int top,
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
