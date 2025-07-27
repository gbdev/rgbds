// SPDX-License-Identifier: MIT

#include "usage.hpp"

#include <stdio.h>
#include <stdlib.h>

void Usage::printAndExit(int code) const {
	fputs(usage, stderr);
	exit(code);
}

void Usage::printAndExit(char const *fmt, ...) const {
	va_list args;
	fputs("FATAL: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);

	printAndExit(1);
}
