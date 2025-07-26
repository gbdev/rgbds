// SPDX-License-Identifier: MIT

#include "link/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>

#include "link/main.hpp"

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",          LEVEL_ALL       },
        {"everything",   LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"assert",       LEVEL_DEFAULT   },
        {"div",          LEVEL_ALL       },
        {"obsolete",     LEVEL_DEFAULT   },
        {"shift",        LEVEL_ALL       },
        {"shift-amount", LEVEL_ALL       },
        {"truncation",   LEVEL_DEFAULT   },
    },
    .paramWarnings = {},
    .state = DiagnosticsState<WarningID>(),
    .nbErrors = 0,
};
// clang-format on

static void printDiag(
    FileStackNode const *src,
    uint32_t lineNo,
    char const *fmt,
    va_list args,
    char const *type,
    char const *flagfmt,
    char const *flag
) {
	fprintf(stderr, "%s: ", type);
	if (src) {
		src->dump(lineNo);
		fputs(": ", stderr);
	}
	if (flagfmt) {
		fprintf(stderr, flagfmt, flag);
		fputs("\n    ", stderr);
	}
	vfprintf(stderr, fmt, args);
	putc('\n', stderr);
}

[[noreturn]]
static void abortLinking(char const *verb) {
	fprintf(
	    stderr,
	    "Linking %s with %" PRIu64 " error%s\n",
	    verb ? verb : "aborted",
	    warnings.nbErrors,
	    warnings.nbErrors == 1 ? "" : "s"
	);
	exit(1);
}

void warning(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "warning", nullptr, 0);
	va_end(args);
}

void warning(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "warning", nullptr, 0);
	va_end(args);
}

void error(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "error", nullptr, 0);
	va_end(args);

	warnings.incrementErrors();
}

void error(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "error", nullptr, 0);
	va_end(args);

	warnings.incrementErrors();
}

void errorNoDump(char const *fmt, ...) {
	va_list args;
	fputs("error: ", stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	warnings.incrementErrors();
}

void argErr(char flag, char const *fmt, ...) {
	va_list args;
	fprintf(stderr, "error: Invalid argument for option '%c': ", flag);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);

	warnings.incrementErrors();
}

[[noreturn]]
void fatal(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "FATAL", nullptr, 0);
	va_end(args);

	warnings.incrementErrors();
	abortLinking(nullptr);
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "FATAL", nullptr, 0);
	va_end(args);

	warnings.incrementErrors();
	abortLinking(nullptr);
}

void requireZeroErrors() {
	if (warnings.nbErrors != 0) {
		abortLinking("failed");
	}
}

void warning(FileStackNode const *src, uint32_t lineNo, WarningID id, char const *fmt, ...) {
	char const *flag = warnings.warningFlags[id].name;
	va_list args;

	va_start(args, fmt);

	switch (warnings.getWarningBehavior(id)) {
	case WarningBehavior::DISABLED:
		break;

	case WarningBehavior::ENABLED:
		printDiag(src, lineNo, fmt, args, "warning", "[-W%s]", flag);
		break;

	case WarningBehavior::ERROR:
		printDiag(src, lineNo, fmt, args, "error", "[-Werror=%s]", flag);

		warnings.incrementErrors();
		break;
	}

	va_end(args);
}
