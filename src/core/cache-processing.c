#include "cgit.h"
#include "cache.h"
#include "cache-internal.h"
#include "html.h"

/* Crude implementation of 32-bit FNV-1 hash algorithm,
 * see http://www.isthe.com/chongo/tech/comp/fnv/ for details
 * about the magic numbers.
 */
#define FNV_OFFSET 0x811c9dc5
#define FNV_PRIME  0x01000193

unsigned long hash_str(const char *str)
{
	unsigned long h = FNV_OFFSET;
	unsigned char *s = (unsigned char *)str;

	if (!s)
		return h;

	while (*s) {
		h *= FNV_PRIME;
		h ^= *s++;
	}
	return h;
}

static int process_slot(struct cache_slot *slot)
{
	int err;

	err = cache_open_slot(slot);
	if (!err && slot->match) {
		if (cache_is_expired(slot)) {
			if (!cache_lock_slot(slot)) {
				/* If the cachefile has been replaced between
				 * `open_slot` and `lock_slot`, we'll just
				 * serve the stale content from the original
				 * cachefile. This way we avoid pruning the
				 * newly generated slot. The same code-path
				 * is chosen if cache_fill_slot() fails for some
				 * reason.
				 *
				 * TODO? check if the new slot contains the
				 * same key as the old one, since we would
				 * prefer to serve the newest content.
				 * This will require us to open yet another
				 * file-descriptor and read and compare the
				 * key from the new file, so for now we're
				 * lazy and just ignore the new file.
				 */
				if (cache_is_modified(slot) || cache_fill_slot(slot)) {
					cache_unlock_slot(slot, 0);
					cache_close_lock(slot);
				} else {
					cache_close_slot(slot);
					cache_unlock_slot(slot, 1);
					slot->cache_fd = slot->lock_fd;
				}
			}
		}
		if ((err = cache_print_slot(slot)) != 0) {
			cache_log("[cgit] error printing cache %s: %s (%d)\n",
				  slot->cache_name,
				  strerror(err),
				  err);
		}
		cache_close_slot(slot);
		return err;
	}

	/* If the cache slot does not exist (or its key doesn't match the
	 * current key), lets try to create a new cache slot for this
	 * request. If this fails (for whatever reason), lets just generate
	 * the content without caching it and fool the caller to believe
	 * everything worked out (but print a warning on stdout).
	 */

	cache_close_slot(slot);
	if ((err = cache_lock_slot(slot)) != 0) {
		cache_log("[cgit] Unable to lock slot %s: %s (%d)\n",
			  slot->lock_name, strerror(err), err);
		slot->fn();
		return 0;
	}

	if ((err = cache_fill_slot(slot)) != 0) {
		cache_log("[cgit] Unable to fill slot %s: %s (%d)\n",
			  slot->lock_name, strerror(err), err);
		cache_unlock_slot(slot, 0);
		cache_close_lock(slot);
		slot->fn();
		return 0;
	}
	// We've got a valid cache slot in the lock file, which
	// is about to replace the old cache slot. But if we
	// release the lockfile and then try to open the new cache
	// slot, we might get a race condition with a concurrent
	// writer for the same cache slot (with a different key).
	// Lets avoid such a race by just printing the content of
	// the lock file.
	slot->cache_fd = slot->lock_fd;
	cache_unlock_slot(slot, 1);
	if ((err = cache_print_slot(slot)) != 0) {
		cache_log("[cgit] error printing cache %s: %s (%d)\n",
			  slot->cache_name,
			  strerror(err),
			  err);
	}
	cache_close_slot(slot);
	return err;
}

/* Print cached content to stdout, generate the content if necessary. */
int cache_process(int size, const char *path, const char *key, int ttl,
		  cache_fill_fn fn)
{
	unsigned long hash;
	int i;
	struct strbuf filename = STRBUF_INIT;
	struct strbuf lockname = STRBUF_INIT;
	struct cache_slot slot;
	int result;

	/* If the cache is disabled, just generate the content */
	if (size <= 0 || ttl == 0) {
		fn();
		return 0;
	}

	/* Verify input, calculate filenames */
	if (!path) {
		cache_log("[cgit] Cache path not specified, caching is disabled\n");
		fn();
		return 0;
	}
	if (!key)
		key = "";
	hash = hash_str(key) % size;
	strbuf_addstr(&filename, path);
	strbuf_ensure_end(&filename, '/');
	for (i = 0; i < 8; i++) {
		strbuf_addf(&filename, "%x", (unsigned char)(hash & 0xf));
		hash >>= 4;
	}
	strbuf_addbuf(&lockname, &filename);
	strbuf_addstr(&lockname, ".lock");
	slot.fn = fn;
	slot.ttl = ttl;
	slot.stdout_fd = -1;
	slot.cache_name = filename.buf;
	slot.lock_name = lockname.buf;
	slot.key = key;
	slot.keylen = strlen(key);
	result = process_slot(&slot);

	strbuf_release(&filename);
	strbuf_release(&lockname);
	return result;
}

/* Return a strftime formatted date/time
 * NB: the result from this function is to shared memory
 */
static char *sprintftime(const char *format, time_t time)
{
	static char buf[64];
	struct tm tm;

	if (!time)
		return NULL;
	gmtime_r(&time, &tm);
	strftime(buf, sizeof(buf)-1, format, &tm);
	return buf;
}

int cache_ls(const char *path)
{
	DIR *dir;
	struct dirent *ent;
	int err = 0;
	struct cache_slot slot = { NULL };
	struct strbuf fullname = STRBUF_INIT;
	size_t prefixlen;

	if (!path) {
		cache_log("[cgit] cache path not specified\n");
		return -1;
	}
	dir = opendir(path);
	if (!dir) {
		err = errno;
		cache_log("[cgit] unable to open path %s: %s (%d)\n",
			  path, strerror(err), err);
		return err;
	}
	strbuf_addstr(&fullname, path);
	strbuf_ensure_end(&fullname, '/');
	prefixlen = fullname.len;
	while ((ent = readdir(dir)) != NULL) {
		if (strlen(ent->d_name) != 8)
			continue;
		strbuf_setlen(&fullname, prefixlen);
		strbuf_addstr(&fullname, ent->d_name);
		slot.cache_name = fullname.buf;
		if ((err = cache_open_slot(&slot)) != 0) {
			cache_log("[cgit] unable to open path %s: %s (%d)\n",
				  fullname.buf, strerror(err), err);
			continue;
		}
		htmlf("%s %s %10"PRIuMAX" %s\n",
		      fullname.buf,
		      sprintftime("%Y-%m-%d %H:%M:%S",
				  slot.cache_st.st_mtime),
		      (uintmax_t)slot.cache_st.st_size,
		      slot.buf);
		cache_close_slot(&slot);
	}
	closedir(dir);
	strbuf_release(&fullname);
	return 0;
}

/* Print a message to stdout */
void cache_log(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

