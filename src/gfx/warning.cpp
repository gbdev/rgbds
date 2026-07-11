// SPDX-License-Identifier: MIT

#include "gfx/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostics.hpp"
#include "style.hpp"

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",           LEVEL_ALL       },
        {"everything",    LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"embedded",      LEVEL_EVERYTHING},
        {"obsolete",      LEVEL_DEFAULT   },
        {"trim-nonempty", LEVEL_ALL       },
    },
    .paramWarnings = {},
    .state = DiagnosticsState<WarningID>(),
    .nbErrors = 0,
};
// clang-format on

[[noreturn]]
void giveUp() {
	style_Set(stderr, STYLE_RED, true);
	fprintf(
	    stderr,
	    "Conversion aborted after %" PRIu64 " error%s\n",
	    warnings.nbErrors,
	    warnings.nbErrors == 1 ? "" : "s"
	);
	style_Reset(stderr);
	exit(1);
}

void requireZeroErrors() {
	if (warnings.nbErrors != 0) {
		giveUp();
	}
}

void error(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	verrorx(fmt, args);
	va_end(args);

	warnings.incrementErrors();
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfatalx(fmt, args);
	va_end(args);

	warnings.incrementErrors();
	giveUp();
}

void warning(WarningID id, char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	WarningBehavior behavior = printDiagnostic(warnings, id, fmt, args);
	va_end(args);

	if (behavior == WarningBehavior::ERROR) {
		warnings.incrementErrors();
	}
}
