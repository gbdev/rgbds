// SPDX-License-Identifier: MIT

#include "gfx/warning.hpp"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",           LEVEL_ALL       },
        {"everything",    LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"embedded",      LEVEL_EVERYTHING},
        {"trim-nonempty", LEVEL_ALL       },
    },
    .paramWarnings = {},
    .state = DiagnosticsState<WarningID>(),
    .nbErrors = 0,
};
// clang-format on

[[noreturn]]
void giveUp() {
	fprintf(
	    stderr,
	    "Conversion aborted after %" PRIu64 " error%s\n",
	    warnings.nbErrors,
	    warnings.nbErrors == 1 ? "" : "s"
	);
	exit(1);
}

void requireZeroErrors() {
	if (warnings.nbErrors != 0) {
		giveUp();
	}
}

void error(char const *fmt, ...) {
	va_list ap;
	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	warnings.incrementErrors();
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list ap;
	fputs("FATAL: ", stderr);
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
		fprintf(stderr, "warning: [-W%s]\n    ", flag);
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		putc('\n', stderr);
		break;

	case WarningBehavior::ERROR:
		fprintf(stderr, "error: [-Werror=%s]\n    ", flag);
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
		putc('\n', stderr);

		warnings.incrementErrors();
		break;
	}
}
