#include "cgit.h"
#include "filter-internal.h"

static int open_exec_filter(struct cgit_filter *base, va_list ap)
{
	struct cgit_exec_filter *filter = (struct cgit_exec_filter *)base;
	int pipe_fh[2];
	int i;

	for (i = 0; i < filter->base.argument_count; i++)
		filter->argv[i + 1] = va_arg(ap, char *);

	filter->old_stdout = chk_positive(dup(STDOUT_FILENO),
		"Unable to duplicate STDOUT");
	chk_zero(pipe(pipe_fh), "Unable to create pipe to subprocess");
	filter->pid = chk_non_negative(fork(), "Unable to create subprocess");
	if (filter->pid == 0) {
		close(pipe_fh[1]);
		chk_non_negative(dup2(pipe_fh[0], STDIN_FILENO),
			"Unable to use pipe as STDIN");
		execvp(filter->cmd, filter->argv);
		die_errno("Unable to exec subprocess %s", filter->cmd);
	}
	close(pipe_fh[0]);
	chk_non_negative(dup2(pipe_fh[1], STDOUT_FILENO),
		"Unable to use pipe as STDOUT");
	close(pipe_fh[1]);
	return 0;
}

static int close_exec_filter(struct cgit_filter *base)
{
	struct cgit_exec_filter *filter = (struct cgit_exec_filter *)base;
	int i, exit_status = 0;

	chk_non_negative(dup2(filter->old_stdout, STDOUT_FILENO),
		"Unable to restore STDOUT");
	close(filter->old_stdout);
	if (filter->pid < 0)
		goto done;
	waitpid(filter->pid, &exit_status, 0);
	if (WIFEXITED(exit_status))
		goto done;
	die("Subprocess %s exited abnormally", filter->cmd);

done:
	for (i = 0; i < filter->base.argument_count; i++)
		filter->argv[i + 1] = NULL;
	return WEXITSTATUS(exit_status);

}

static void fprintf_exec_filter(struct cgit_filter *base, FILE *f, const char *prefix)
{
	struct cgit_exec_filter *filter = (struct cgit_exec_filter *)base;
	fprintf(f, "%sexec:%s\n", prefix, filter->cmd);
}

static void cleanup_exec_filter(struct cgit_filter *base)
{
	struct cgit_exec_filter *filter = (struct cgit_exec_filter *)base;
	if (filter->argv) {
		free(filter->argv);
		filter->argv = NULL;
	}
	if (filter->cmd) {
		free(filter->cmd);
		filter->cmd = NULL;
	}
}

struct cgit_filter *cgit_new_exec_filter(const char *cmd, int argument_count)
{
	struct cgit_exec_filter *f;
	int args_size = 0;

	f = xmalloc(sizeof(*f));
	/* We leave argv for now and assign it below. */
	cgit_exec_filter_init(f, xstrdup(cmd), NULL);
	f->base.argument_count = argument_count;
	args_size = (2 + argument_count) * sizeof(char *);
	f->argv = xmalloc(args_size);
	memset(f->argv, 0, args_size);
	f->argv[0] = f->cmd;
	return &f->base;
}

void cgit_exec_filter_init(struct cgit_exec_filter *filter, char *cmd, char **argv)
{
	memset(filter, 0, sizeof(*filter));
	filter->base.open = open_exec_filter;
	filter->base.close = close_exec_filter;
	filter->base.fprintfp = fprintf_exec_filter;
	filter->base.cleanup = cleanup_exec_filter;
	filter->cmd = cmd;
	filter->argv = argv;
	/* The argument count for open_filter is zero by default, unless called from new_filter, above. */
	filter->base.argument_count = 0;
}
