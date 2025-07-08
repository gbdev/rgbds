// SPDX-License-Identifier: MIT

#include "error.hpp"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void warnx(char const *fmt, ...) {
	va_list ap;
	fputs("warning: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
}

[[noreturn]]
void errx(char const *fmt, ...) {
	va_list ap;
	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
	exit(1);
}
