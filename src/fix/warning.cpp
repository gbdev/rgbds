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
	va_list ap;
	style_Set(stderr, STYLE_RED, true);
	fputs("error: ", stderr);
	style_Reset(stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	warnings.incrementErrors();
}

void fatal(char const *fmt, ...) {
	va_list ap;
	style_Set(stderr, STYLE_RED, true);
	fputs("FATAL: ", stderr);
	style_Reset(stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	exit(1);
}

void warning(WarningID id, char const *fmt, ...) {
	char const *flag = warnings.warningFlags[id].name;
	va_list ap;

	switch (warnings.getWarningBehavior(id)) {
	case WarningBehavior::DISABLED:
		break;

	case WarningBehavior::ENABLED:
		style_Set(stderr, STYLE_YELLOW, true);
		fputs("warning: ", stderr);
		style_Reset(stderr);
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		style_Set(stderr, STYLE_YELLOW, true);
		fprintf(stderr, " [-W%s]\n", flag);
		style_Reset(stderr);
		break;

	case WarningBehavior::ERROR:
		style_Set(stderr, STYLE_RED, true);
		fputs("error: ", stderr);
		style_Reset(stderr);
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		style_Set(stderr, STYLE_RED, true);
		fprintf(stderr, " [-Werror=%s]\n", flag);
		style_Reset(stderr);

		warnings.incrementErrors();
		break;
	}
}
