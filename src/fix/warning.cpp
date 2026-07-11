// SPDX-License-Identifier: MIT

#include "fix/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostics.hpp"
#include "style.hpp"

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",        LEVEL_ALL       },
        {"everything", LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"mbc",        LEVEL_DEFAULT   },
        {"obsolete",   LEVEL_DEFAULT   },
        {"overwrite",  LEVEL_DEFAULT   },
        {"sgb",        LEVEL_DEFAULT   },
        {"truncation", LEVEL_DEFAULT   },
    },
    .paramWarnings = {},
    .state = DiagnosticsState<WarningID>(),
    .nbErrors = 0,
};
// clang-format on

uint32_t checkErrors(char const *filename) {
	if (warnings.nbErrors > 0) {
		style_Set(stderr, STYLE_RED, true);
		fprintf(
		    stderr,
		    "Fixing \"%s\" failed with %" PRIu64 " error%s\n",
		    filename,
		    warnings.nbErrors,
		    warnings.nbErrors == 1 ? "" : "s"
		);
		style_Reset(stderr);
	}
	return warnings.nbErrors;
}

void error(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	verrorx(fmt, args);
	va_end(args);

	warnings.incrementErrors();
}

void fatal(char const *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	vfatalx(fmt, args);
	va_end(args);

	exit(1);
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
