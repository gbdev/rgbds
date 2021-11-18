/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2005-2018, Rich Felker and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"

void rgbds_vwarn(char const *fmt, va_list ap)
{
	fprintf(stderr, "warning: ");
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fputs(": ", stderr);
	}
	perror(NULL);
}

void rgbds_vwarnx(char const *fmt, va_list ap)
{
	fprintf(stderr, "warning");
	if (fmt) {
		fputs(": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
}

_Noreturn void rgbds_verr(int status, char const *fmt, va_list ap)
{
	fprintf(stderr, "error: ");
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fputs(": ", stderr);
	}
	fputs(strerror(errno), stderr);
	putc('\n', stderr);
	exit(status);
}

_Noreturn void rgbds_verrx(int status, char const *fmt, va_list ap)
{
	fprintf(stderr, "error");
	if (fmt) {
		fputs(": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
	exit(status);
}

void rgbds_warn(char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void rgbds_warnx(char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

_Noreturn void rgbds_err(int status, char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(status, fmt, ap);
	va_end(ap);
}

_Noreturn void rgbds_errx(int status, char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(status, fmt, ap);
	va_end(ap);
}
