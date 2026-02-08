/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef UI_DIFF_RENDER_INTERNAL_H
#define UI_DIFF_RENDER_INTERNAL_H

#include "cgit.h"

void cgit_diff_set_current_prefix(const char *prefix);
void cgit_diff_set_use_ssdiff(int enabled);
void cgit_print_diffstat(const struct object_id *old_oid,
			 const struct object_id *new_oid,
			 const char *prefix);
void cgit_diff_filepair_cb(struct diff_filepair *pair);

#endif
