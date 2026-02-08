/* cache.c: cache management
 *
 * Copyright (C) 2006-2014 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 *
 *
 * The cache is just a directory structure where each file is a cache slot,
 * and each filename is based on the hash of some key (e.g. the cgit url).
 * Each file contains the full key followed by the cached content for that
 * key.
 *
 */

#include "cgit.h"
#include "cache.h"
#include "cache-internal.h"
#include "html.h"
#ifdef HAVE_LINUX_SENDFILE
#include <sys/sendfile.h>
#endif

/* Open an existing cache slot and fill the cache buffer with
 * (part of) the content of the cache file. Return 0 on success
 * and errno otherwise.
 */
int cache_open_slot(struct cache_slot *slot)
{
	char *bufz;
	ssize_t bufkeylen = -1;

	slot->cache_fd = open(slot->cache_name, O_RDONLY);
	if (slot->cache_fd == -1)
		return errno;

	if (fstat(slot->cache_fd, &slot->cache_st))
		return errno;

	slot->bufsize = xread(slot->cache_fd, slot->buf, sizeof(slot->buf));
	if (slot->bufsize < 0)
		return errno;

	bufz = memchr(slot->buf, 0, slot->bufsize);
	if (bufz)
		bufkeylen = bufz - slot->buf;

	if (slot->key)
		slot->match = bufkeylen == slot->keylen &&
		    !memcmp(slot->key, slot->buf, bufkeylen + 1);

	return 0;
}

/* Close the active cache slot */
int cache_close_slot(struct cache_slot *slot)
{
	int err = 0;
	if (slot->cache_fd > 0) {
		if (close(slot->cache_fd))
			err = errno;
		else
			slot->cache_fd = -1;
	}
	return err;
}

/* Print the content of the active cache slot (but skip the key). */
int cache_print_slot(struct cache_slot *slot)
{
	off_t off;
#ifdef HAVE_LINUX_SENDFILE
	off_t size;
#endif

	off = slot->keylen + 1;

#ifdef HAVE_LINUX_SENDFILE
	size = slot->cache_st.st_size;

	do {
		ssize_t ret;
		ret = sendfile(STDOUT_FILENO, slot->cache_fd, &off, size - off);
		if (ret < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			/* Fall back to read/write on EINVAL or ENOSYS */
			if (errno == EINVAL || errno == ENOSYS)
				break;
			return errno;
		}
		if (off == size)
			return 0;
	} while (1);
#endif

	if (lseek(slot->cache_fd, off, SEEK_SET) != off)
		return errno;

	do {
		ssize_t ret;
		ret = xread(slot->cache_fd, slot->buf, sizeof(slot->buf));
		if (ret < 0)
			return errno;
		if (ret == 0)
			return 0;
		if (write_in_full(STDOUT_FILENO, slot->buf, ret) < 0)
			return errno;
	} while (1);
}

/* Check if the slot has expired */
int cache_is_expired(struct cache_slot *slot)
{
	if (slot->ttl < 0)
		return 0;
	else
		return slot->cache_st.st_mtime + slot->ttl * 60 < time(NULL);
}

/* Check if the slot has been modified since we opened it.
 * NB: If stat() fails, we pretend the file is modified.
 */
int cache_is_modified(struct cache_slot *slot)
{
	struct stat st;

	if (stat(slot->cache_name, &st))
		return 1;
	return (st.st_ino != slot->cache_st.st_ino ||
		st.st_mtime != slot->cache_st.st_mtime ||
		st.st_size != slot->cache_st.st_size);
}

/* Close an open lockfile */
int cache_close_lock(struct cache_slot *slot)
{
	int err = 0;
	if (slot->lock_fd > 0) {
		if (close(slot->lock_fd))
			err = errno;
		else
			slot->lock_fd = -1;
	}
	return err;
}

/* Create a lockfile used to store the generated content for a cache
 * slot, and write the slot key + \0 into it.
 * Returns 0 on success and errno otherwise.
 */
int cache_lock_slot(struct cache_slot *slot)
{
	struct flock lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};

	slot->lock_fd = open(slot->lock_name, O_RDWR | O_CREAT,
			     S_IRUSR | S_IWUSR);
	if (slot->lock_fd == -1)
		return errno;
	if (fcntl(slot->lock_fd, F_SETLK, &lock) < 0) {
		int saved_errno = errno;
		close(slot->lock_fd);
		slot->lock_fd = -1;
		return saved_errno;
	}
	if (xwrite(slot->lock_fd, slot->key, slot->keylen + 1) < 0)
		return errno;
	return 0;
}

/* Release the current lockfile. If `replace_old_slot` is set the
 * lockfile replaces the old cache slot, otherwise the lockfile is
 * just deleted.
 */
int cache_unlock_slot(struct cache_slot *slot, int replace_old_slot)
{
	int err;

	if (replace_old_slot)
		err = rename(slot->lock_name, slot->cache_name);
	else
		err = unlink(slot->lock_name);

	/* Restore stdout and close the temporary FD. */
	if (slot->stdout_fd >= 0) {
		dup2(slot->stdout_fd, STDOUT_FILENO);
		close(slot->stdout_fd);
		slot->stdout_fd = -1;
	}

	if (err)
		return errno;

	return 0;
}

/* Generate the content for the current cache slot by redirecting
 * stdout to the lock-fd and invoking the callback function
 */
int cache_fill_slot(struct cache_slot *slot)
{
	/* Preserve stdout */
	slot->stdout_fd = dup(STDOUT_FILENO);
	if (slot->stdout_fd == -1)
		return errno;

	/* Redirect stdout to lockfile */
	if (dup2(slot->lock_fd, STDOUT_FILENO) == -1)
		return errno;

	/* Generate cache content */
	slot->fn();

	/* Make sure any buffered data is flushed to the file */
	if (fflush(stdout))
		return errno;

	/* update stat info */
	if (fstat(slot->lock_fd, &slot->cache_st))
		return errno;

	return 0;
}
