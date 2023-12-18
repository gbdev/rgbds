/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.hpp"
#include "platform.hpp"

static void vwarn(char const NONNULL(fmt), va_list ap)
{
	const char *error = strerror(errno);

	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", error);
}

static void vwarnx(char const NONNULL(fmt), va_list ap)
{
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
}

[[noreturn]] static void verr(char const NONNULL(fmt), va_list ap)
{
	const char *error = strerror(errno);

	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", error);
	exit(1);
}

[[noreturn]] static void verrx(char const NONNULL(fmt), va_list ap)
{
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
	exit(1);
}

void warn(char const NONNULL(fmt), ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void warnx(char const NONNULL(fmt), ...)
{
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

[[noreturn]] void err(char const NONNULL(fmt), ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(fmt, ap);
	va_end(ap);
}

[[noreturn]] void errx(char const NONNULL(fmt), ...)
{
	va_list ap;

	va_start(ap, fmt);
	verrx(fmt, ap);
	va_end(ap);
}
