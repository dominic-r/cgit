/* ui-shared.c: common web output functions
 *
 * Copyright (C) 2006-2017 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "cgit.h"
#include "ui-shared.h"
#include "html.h"

void cgit_print_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	cgit_vprint_error(fmt, ap);
	va_end(ap);
}

void cgit_vprint_error(const char *fmt, va_list ap)
{
	va_list cp;
	html("<div class='error'>");
	va_copy(cp, ap);
	html_vtxtf(fmt, cp);
	va_end(cp);
	html("</div>\n");
}
