/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef CGIT_CACHE_INTERNAL_H
#define CGIT_CACHE_INTERNAL_H

#include "cache.h"

#define CACHE_BUFSIZE (1024 * 4)

struct cache_slot {
	const char *key;
	size_t keylen;
	int ttl;
	cache_fill_fn fn;
	int cache_fd;
	int lock_fd;
	int stdout_fd;
	const char *cache_name;
	const char *lock_name;
	int match;
	struct stat cache_st;
	int bufsize;
	char buf[CACHE_BUFSIZE];
};

int cache_open_slot(struct cache_slot *slot);
int cache_close_slot(struct cache_slot *slot);
int cache_print_slot(struct cache_slot *slot);
int cache_is_expired(struct cache_slot *slot);
int cache_is_modified(struct cache_slot *slot);
int cache_close_lock(struct cache_slot *slot);
int cache_lock_slot(struct cache_slot *slot);
int cache_unlock_slot(struct cache_slot *slot, int replace_old_slot);
int cache_fill_slot(struct cache_slot *slot);

#endif
