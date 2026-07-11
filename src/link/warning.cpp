// SPDX-License-Identifier: MIT

#include "link/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostics.hpp"
#include "style.hpp"

#include "link/fstack.hpp"
#include "link/lexer.hpp"

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",            LEVEL_ALL       },
        {"everything",     LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"assert",         LEVEL_DEFAULT   },
        {"div",            LEVEL_ALL       },
        {"large-constant", LEVEL_DEFAULT   },
        {"obsolete",       LEVEL_DEFAULT   },
        {"shift",          LEVEL_ALL       },
        {"shift-amount",   LEVEL_ALL       },
        // Parametric warnings
        {"truncation",     LEVEL_DEFAULT   },
        {"truncation",     LEVEL_EVERYTHING},
    },
    .paramWarnings = {
        {WARNING_TRUNCATION_1, WARNING_TRUNCATION_2, 1},
    },
    .state = DiagnosticsState<WarningID>(),
    .nbErrors = 0,
};
// clang-format on

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
	vwarnx(fmt, args);
	va_end(args);
	if (src) {
		src->printBacktrace(lineNo);
	}
}

void warning(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vwarnx(fmt, args);
	va_end(args);
}

void error(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	verrorx(fmt, args);
	va_end(args);

	if (src) {
		src->printBacktrace(lineNo);
	}
	warnings.incrementErrors();
}

void error(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	verrorx(fmt, args);
	va_end(args);

	warnings.incrementErrors();
}

void scriptError(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	verrorx(fmt, args);
	va_end(args);

	lexer_TraceCurrent();
	warnings.incrementErrors();
}

[[noreturn]]
void fatal(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfatalx(fmt, args);
	va_end(args);

	if (src) {
		src->printBacktrace(lineNo);
	}
	warnings.incrementErrors();
	abortLinking(nullptr);
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfatalx(fmt, args);
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
	va_start(args, fmt);
	vfatalx(fmt, args);
	va_end(args);

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
	va_list args;
	va_start(args, fmt);
	WarningBehavior behavior = printDiagnostic(warnings, id, fmt, args);
	va_end(args);

	if (behavior != WarningBehavior::DISABLED) {
		if (src) {
			src->printBacktrace(lineNo);
		}
		if (behavior == WarningBehavior::ERROR) {
			warnings.incrementErrors();
		}
	}
}

void scriptWarning(WarningID id, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	WarningBehavior behavior = printDiagnostic(warnings, id, fmt, args);
	va_end(args);

	if (behavior != WarningBehavior::DISABLED) {
		lexer_TraceCurrent();
		if (behavior == WarningBehavior::ERROR) {
			warnings.incrementErrors();
		}
	}
}
