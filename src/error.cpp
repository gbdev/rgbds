/* SPDX-License-Identifier: MIT */

#include "error.hpp"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void vwarn(char const *fmt, va_list ap) {
	char const *error = strerror(errno);

	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", error);
}

static void vwarnx(char const *fmt, va_list ap) {
	fprintf(stderr, "warning: ");
	vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
}

[[noreturn]] static void verr(char const *fmt, va_list ap) {
	char const *error = strerror(errno);

	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", error);
	va_end(ap);
	exit(1);
}

[[noreturn]] static void verrx(char const *fmt, va_list ap) {
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
	va_end(ap);
	exit(1);
}

void warn(char const *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vwarn(fmt, ap);
	va_end(ap);
}

void warnx(char const *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

[[noreturn]] void err(char const *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	verr(fmt, ap);
}

[[noreturn]] void errx(char const *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	verrx(fmt, ap);
}
