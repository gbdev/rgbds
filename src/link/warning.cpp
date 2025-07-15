// SPDX-License-Identifier: MIT

#include "link/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>

#include "link/main.hpp"

static uint32_t nbErrors = 0;

static void printDiag(
    FileStackNode const *src, uint32_t lineNo, char const *fmt, va_list args, char const *type
) {
	fprintf(stderr, "%s: ", type);
	if (src) {
		src->dump(lineNo);
		fputs(": ", stderr);
	}
	vfprintf(stderr, fmt, args);
	putc('\n', stderr);
}

static void incrementErrors() {
	if (nbErrors != UINT32_MAX) {
		nbErrors++;
	}
}

[[noreturn]]
static void abortLinking(char const *verb) {
	fprintf(
	    stderr,
	    "Linking %s with %" PRIu32 " error%s\n",
	    verb ? verb : "aborted",
	    nbErrors,
	    nbErrors == 1 ? "" : "s"
	);
	exit(1);
}

void warning(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "warning");
	va_end(args);
}

void warning(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "warning");
	va_end(args);
}

void error(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "error");
	va_end(args);

	incrementErrors();
}

void error(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "error");
	va_end(args);

	incrementErrors();
}

void errorNoDump(char const *fmt, ...) {
	va_list args;
	fputs("error: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	incrementErrors();
}

void argErr(char flag, char const *fmt, ...) {
	va_list args;
	fprintf(stderr, "error: Invalid argument for option '%c': ", flag);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);

	incrementErrors();
}

[[noreturn]]
void fatal(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "FATAL");
	va_end(args);

	incrementErrors();
	abortLinking(nullptr);
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "FATAL");
	va_end(args);

	incrementErrors();
	abortLinking(nullptr);
}

void requireZeroErrors() {
	if (nbErrors != 0) {
		abortLinking("failed");
	}
}
