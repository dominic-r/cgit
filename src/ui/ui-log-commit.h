#ifndef UI_LOG_COMMIT_INTERNAL_H
#define UI_LOG_COMMIT_INTERNAL_H

#include "cgit.h"

void cgit_log_reset_lines_counted(void);
int cgit_log_show_commit(struct commit *commit, struct rev_info *revs);
void cgit_log_print_commit(struct commit *commit, struct rev_info *revs);

#endif
