// SPDX-License-Identifier: MIT

#include "link/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>

#include "style.hpp"

#include "link/fstack.hpp"
#include "link/lexer.hpp"

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
    .traceDepth = 0,
    .nbErrors = 0,
};
// clang-format on

static void printDiag(
    FileStackNode const *src,
    uint32_t lineNo,
    char const *fmt,
    va_list args,
    char const *type,
    StyleColor color,
    char const *flagfmt,
    char const *flag
) {
	style_Set(stderr, color, true);
	fprintf(stderr, "%s: ", type);
	style_Reset(stderr);
	vfprintf(stderr, fmt, args);
	if (flagfmt) {
		style_Set(stderr, color, true);
		putc(' ', stderr);
		fprintf(stderr, flagfmt, flag);
	}
	putc('\n', stderr);

	if (src) {
		src->printBacktrace(lineNo);
	}
}

[[noreturn]]
static void abortLinking(char const *verb) {
	style_Set(stderr, STYLE_RED, true);
	fprintf(
	    stderr,
	    "Linking %s with %" PRIu64 " error%s\n",
	    verb ? verb : "aborted",
	    warnings.nbErrors,
	    warnings.nbErrors == 1 ? "" : "s"
	);
	style_Reset(stderr);
	exit(1);
}

void warning(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "warning", STYLE_YELLOW, nullptr, nullptr);
	va_end(args);
}

void warning(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "warning", STYLE_YELLOW, nullptr, nullptr);
	va_end(args);
}

void error(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "error", STYLE_RED, nullptr, nullptr);
	va_end(args);

	warnings.incrementErrors();
}

void error(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "error", STYLE_RED, nullptr, nullptr);
	va_end(args);

	warnings.incrementErrors();
}

void scriptError(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "error", STYLE_RED, nullptr, nullptr);
	va_end(args);

	lexer_TraceCurrent();

	warnings.incrementErrors();
}

[[noreturn]]
void fatal(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(src, lineNo, fmt, args, "FATAL", STYLE_RED, nullptr, nullptr);
	va_end(args);

	warnings.incrementErrors();
	abortLinking(nullptr);
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	printDiag(nullptr, 0, fmt, args, "FATAL", STYLE_RED, nullptr, nullptr);
	va_end(args);

	warnings.incrementErrors();
	abortLinking(nullptr);
}

[[noreturn]]
void fatalTwo(
    FileStackNode const &src1,
    uint32_t lineNo1,
    FileStackNode const &src2,
    uint32_t lineNo2,
    char const *fmt,
    ...
) {
	va_list args;
	style_Set(stderr, STYLE_RED, true);
	fputs("FATAL: ", stderr);
	style_Reset(stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);

	src1.printBacktrace(lineNo1);
	fputs("    and also:\n", stderr);
	src2.printBacktrace(lineNo2);

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
		printDiag(src, lineNo, fmt, args, "warning", STYLE_RED, "[-W%s]", flag);
		break;

	case WarningBehavior::ERROR:
		printDiag(src, lineNo, fmt, args, "error", STYLE_YELLOW, "[-Werror=%s]", flag);

		warnings.incrementErrors();
		break;
	}

	va_end(args);
}
