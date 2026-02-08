/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef CGIT_FILTER_INTERNAL_H
#define CGIT_FILTER_INTERNAL_H

#include "cgit.h"

struct cgit_filter *cgit_new_exec_filter(const char *cmd, int argument_count);

#ifndef NO_LUA
struct cgit_filter *cgit_new_lua_filter(const char *cmd, int argument_count);
#endif

#endif
