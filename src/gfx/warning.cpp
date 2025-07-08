// SPDX-License-Identifier: MIT

#include "gfx/warning.hpp"

#include <limits>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uintmax_t nbErrors;

[[noreturn]]
void giveUp() {
	fprintf(stderr, "Conversion aborted after %ju error%s\n", nbErrors, nbErrors == 1 ? "" : "s");
	exit(1);
}

void requireZeroErrors() {
	if (nbErrors != 0) {
		giveUp();
	}
}

void error(char const *fmt, ...) {
	va_list ap;
	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max()) {
		nbErrors++;
	}
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list ap;
	fputs("FATAL: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max()) {
		nbErrors++;
	}

	giveUp();
}
