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

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list ap;
	style_Set(stderr, STYLE_RED, true);
	fputs("FATAL: ", stderr);
	style_Reset(stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	warnings.incrementErrors();
	giveUp();
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
