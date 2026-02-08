/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef CMD_H
#define CMD_H

typedef void (*cgit_cmd_fn)(void);

struct cgit_cmd {
	const char *name;
	cgit_cmd_fn fn;
	unsigned int want_repo:1,
		want_vpath:1,
		is_clone:1;
};

extern struct cgit_cmd *cgit_get_cmd(void);

#endif /* CMD_H */
