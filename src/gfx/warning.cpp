// SPDX-License-Identifier: MIT

#include "gfx/warning.hpp"

#include <limits>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uintmax_t nbErrors;

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
};
// clang-format on

[[noreturn]]
void giveUp() {
	fprintf(stderr, "Conversion aborted after %ju error%s\n", nbErrors, nbErrors == 1 ? "" : "s");
	exit(1);
}

void requireZeroErrors() {
	if (nbErrors != 0) {
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

	if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max()) {
		nbErrors++;
	}
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list ap;
	fputs("FATAL: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max()) {
		nbErrors++;
	}

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

		if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max()) {
			nbErrors++;
		}
		break;
	}
}
