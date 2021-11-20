/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2005-2021, Rich Felker and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

static void vwarn(char const *fmt, va_list ap)
{
	fprintf(stderr, "warning: ");
	if (fmt) {
		vfprintf(stderr, fmt, ap);
		fputs(": ", stderr);
	}
	perror(NULL);
}

static void vwarnx(char const *fmt, va_list ap)
{
	fprintf(stderr, "warning");
	if (fmt) {
		fputs(": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
}

_Noreturn static void verr(int status, char const *fmt, va_list ap)
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

_Noreturn static void verrx(int status, char const *fmt, va_list ap)
{
	fprintf(stderr, "error");
	if (fmt) {
		fputs(": ", stderr);
		vfprintf(stderr, fmt, ap);
	}
	putc('\n', stderr);
	exit(status);
}

void warn(char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void warnx(char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

_Noreturn void err(int status, char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(status, fmt, ap);
	va_end(ap);
}

_Noreturn void errx(int status, char const *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(status, fmt, ap);
	va_end(ap);
}
