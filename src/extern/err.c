/*
 * Copyright © 2005-2013 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "extern/err.h"

void rgbds_vwarn(const char *fmt, va_list ap)
{
	fprintf (stderr, "warning");
	if (fmt) {
		fputs (": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
	perror(0);
}

void rgbds_vwarnx(const char *fmt, va_list ap)
{
	fprintf (stderr, "warning");
	if (fmt) {
		fputs (": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
}

noreturn void rgbds_verr(int status, const char *fmt, va_list ap)
{
	fprintf (stderr, "error");
	if (fmt) {
		fputs (": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
	exit(status);
}

noreturn void rgbds_verrx(int status, const char *fmt, va_list ap)
{
	fprintf (stderr, "error");
        if (fmt) {
                fputs (": ", stderr);
                vfprintf(stderr, fmt, ap);
        }
	putc('\n', stderr);
	exit(status);
}

void rgbds_warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void rgbds_warnx(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

noreturn void rgbds_err(int status, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verr(status, fmt, ap);
	va_end(ap);
}

noreturn void rgbds_errx(int status, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verrx(status, fmt, ap);
	va_end(ap);
}
