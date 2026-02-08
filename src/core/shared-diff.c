#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"

void cgit_diff_tree_cb(struct diff_queue_struct *q,
		       struct diff_options *options, void *data)
{
	int i;

	for (i = 0; i < q->nr; i++) {
		if (q->queue[i]->status == 'U')
			continue;
		((filepair_fn)data)(q->queue[i]);
	}
}

static int load_mmfile(mmfile_t *file, const struct object_id *oid)
{
	enum object_type type;

	if (is_null_oid(oid)) {
		file->ptr = (char *)"";
		file->size = 0;
	} else {
		file->ptr = odb_read_object(the_repository->objects, oid, &type,
		                           (unsigned long *)&file->size);
	}
	return 1;
}

/*
 * Receive diff-buffers from xdiff and concatenate them as
 * needed across multiple callbacks.
 *
 * This is basically a copy of xdiff-interface.c/xdiff_outf(),
 * ripped from git and modified to use globals instead of
 * a special callback-struct.
 */
static char *diffbuf = NULL;
static int buflen = 0;

static int filediff_cb(void *priv, mmbuffer_t *mb, int nbuf)
{
	int i;

	for (i = 0; i < nbuf; i++) {
		if (mb[i].ptr[mb[i].size-1] != '\n') {
			/* Incomplete line */
			diffbuf = xrealloc(diffbuf, buflen + mb[i].size);
			memcpy(diffbuf + buflen, mb[i].ptr, mb[i].size);
			buflen += mb[i].size;
			continue;
		}

		/* we have a complete line */
		if (!diffbuf) {
			((linediff_fn)priv)(mb[i].ptr, mb[i].size);
			continue;
		}
		diffbuf = xrealloc(diffbuf, buflen + mb[i].size);
		memcpy(diffbuf + buflen, mb[i].ptr, mb[i].size);
		((linediff_fn)priv)(diffbuf, buflen + mb[i].size);
		free(diffbuf);
		diffbuf = NULL;
		buflen = 0;
	}
	if (diffbuf) {
		((linediff_fn)priv)(diffbuf, buflen);
		free(diffbuf);
		diffbuf = NULL;
		buflen = 0;
	}
	return 0;
}

int cgit_diff_files(const struct object_id *old_oid,
		    const struct object_id *new_oid, unsigned long *old_size,
		    unsigned long *new_size, int *binary, int context,
		    int ignorews, linediff_fn fn)
{
	mmfile_t file1, file2;
	xpparam_t diff_params;
	xdemitconf_t emit_params;
	xdemitcb_t emit_cb;

	if (!load_mmfile(&file1, old_oid) || !load_mmfile(&file2, new_oid))
		return 1;

	*old_size = file1.size;
	*new_size = file2.size;

	if ((file1.ptr && buffer_is_binary(file1.ptr, file1.size)) ||
	    (file2.ptr && buffer_is_binary(file2.ptr, file2.size))) {
		*binary = 1;
		if (file1.size)
			free(file1.ptr);
		if (file2.size)
			free(file2.ptr);
		return 0;
	}

	memset(&diff_params, 0, sizeof(diff_params));
	memset(&emit_params, 0, sizeof(emit_params));
	memset(&emit_cb, 0, sizeof(emit_cb));
	diff_params.flags = XDF_NEED_MINIMAL;
	if (ignorews)
		diff_params.flags |= XDF_IGNORE_WHITESPACE;
	emit_params.ctxlen = context > 0 ? context : 3;
	emit_params.flags = XDL_EMIT_FUNCNAMES;
	emit_cb.out_line = filediff_cb;
	emit_cb.priv = fn;
	xdl_diff(&file1, &file2, &diff_params, &emit_params, &emit_cb);
	if (file1.size)
		free(file1.ptr);
	if (file2.size)
		free(file2.ptr);
	return 0;
}

void cgit_diff_tree(const struct object_id *old_oid,
		    const struct object_id *new_oid,
		    filepair_fn fn, const char *prefix, int ignorews)
{
	struct diff_options opt;
	struct pathspec_item *item;

	repo_diff_setup(the_repository, &opt);
	opt.output_format = DIFF_FORMAT_CALLBACK;
	opt.detect_rename = 1;
	opt.rename_limit = ctx.cfg.renamelimit;
	opt.flags.recursive = 1;
	if (ignorews)
		DIFF_XDL_SET(&opt, IGNORE_WHITESPACE);
	opt.format_callback = cgit_diff_tree_cb;
	opt.format_callback_data = fn;
	if (prefix) {
		item = xcalloc(1, sizeof(*item));
		item->match = xstrdup(prefix);
		item->len = strlen(prefix);
		opt.pathspec.nr = 1;
		opt.pathspec.items = item;
	}
	diff_setup_done(&opt);

	if (old_oid && !is_null_oid(old_oid))
		diff_tree_oid(old_oid, new_oid, "", &opt);
	else
		diff_root_tree_oid(new_oid, "", &opt);
	diffcore_std(&opt);
	diff_flush(&opt);
}

void cgit_diff_commit(struct commit *commit, filepair_fn fn, const char *prefix)
{
	const struct object_id *old_oid = NULL;

	if (commit->parents)
		old_oid = &commit->parents->item->object.oid;
	cgit_diff_tree(old_oid, &commit->object.oid, fn, prefix,
		       ctx.qry.ignorews);
}
