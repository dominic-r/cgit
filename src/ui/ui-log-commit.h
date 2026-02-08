/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef UI_LOG_COMMIT_INTERNAL_H
#define UI_LOG_COMMIT_INTERNAL_H

#include "cgit.h"

void cgit_log_reset_lines_counted(void);
int cgit_log_show_commit(struct commit *commit, struct rev_info *revs);
void cgit_log_print_commit(struct commit *commit, struct rev_info *revs);

#endif
