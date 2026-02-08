/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef UI_STATS_RENDER_INTERNAL_H
#define UI_STATS_RENDER_INTERNAL_H

#include "cgit.h"
#include "ui-stats.h"

struct authorstat {
	long total;
	struct string_list list;
};

void cgit_print_authors_table(struct string_list *authors, int top,
			      const struct cgit_period *period);

#endif
