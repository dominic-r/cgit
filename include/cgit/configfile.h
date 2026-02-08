/* Copyright (C) Dominic R and contributors (see AUTHORS)
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "cgit.h"

typedef void (*configfile_value_fn)(const char *name, const char *value);

extern int parse_configfile(const char *filename, configfile_value_fn fn);

#endif /* CONFIGFILE_H */
