// SPDX-License-Identifier: MIT

#include "link/warning.hpp"

#include <inttypes.h>

#include "link/main.hpp"

static uint32_t nbErrors = 0;

static void printDiag(
    char const *fmt, va_list args, char const *type, FileStackNode const *where, uint32_t lineNo
) {
	fputs(type, stderr);
	fputs(": ", stderr);
	if (where) {
		where->dump(lineNo);
		fputs(": ", stderr);
	}
	vfprintf(stderr, fmt, args);
	putc('\n', stderr);
}

void warning(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(fmt, args, "warning", where, lineNo);
	va_end(args);
}

void error(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(fmt, args, "error", where, lineNo);
	va_end(args);

	if (nbErrors != UINT32_MAX) {
		nbErrors++;
	}
}

void errorNoDump(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	fputs("error: ", stderr);
	if (where) {
		where->dump(lineNo);
		fputs(": ", stderr);
	}
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	if (nbErrors != UINT32_MAX) {
		nbErrors++;
	}
}

void argErr(char flag, char const *fmt, ...) {
	va_list args;
	fprintf(stderr, "error: Invalid argument for option '%c': ", flag);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);

	if (nbErrors != UINT32_MAX) {
		nbErrors++;
	}
}

[[noreturn]]
void fatal(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(fmt, args, "FATAL", where, lineNo);
	va_end(args);

	if (nbErrors != UINT32_MAX) {
		nbErrors++;
	}

	fprintf(
	    stderr, "Linking aborted after %" PRIu32 " error%s\n", nbErrors, nbErrors == 1 ? "" : "s"
	);
	exit(1);
}

void requireZeroErrors() {
	if (nbErrors != 0) {
		fprintf(
		    stderr, "Linking failed with %" PRIu32 " error%s\n", nbErrors, nbErrors == 1 ? "" : "s"
		);
		exit(1);
	}
}
