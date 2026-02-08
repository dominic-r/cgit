#ifndef CGIT_MAIN_INTERNAL_H
#define CGIT_MAIN_INTERNAL_H

#include "cgit.h"

void cgit_prepare_context(void);
void cgit_parse_args(int argc, const char **argv);

void cgit_repo_config(struct cgit_repo *repo, const char *name,
		      const char *value);
void cgit_parse_config_file(const char *path);
void cgit_parse_querystring(void);
void cgit_process_cached_repolist(const char *path);

void cgit_repo_setup_env(int *nongit);
int cgit_repo_prepare_cmd(int nongit);

void cgit_authenticate_cookie(void);
void cgit_auth_print_body(void);

#endif
