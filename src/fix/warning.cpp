#include "fix/warning.hpp"

static uint32_t nbErrors;

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",        LEVEL_ALL       },
        {"everything", LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"mbc",        LEVEL_DEFAULT   },
        {"overwrite",  LEVEL_DEFAULT   },
        {"truncation", LEVEL_DEFAULT   },
    },
    .paramWarnings = {},
    .state = DiagnosticsState<WarningID>(),
};
// clang-format on

void error(char const *fmt, ...) {
	va_list ap;
	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != UINT32_MAX) {
		++nbErrors;
	}
}

void fatal(char const *fmt, ...) {
	va_list ap;
	fputs("FATAL: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != UINT32_MAX) {
		++nbErrors;
	}
}

void resetErrors() {
	nbErrors = 0;
}

uint32_t checkErrors(char const *filename) {
	if (nbErrors > 0) {
		fprintf(
		    stderr,
		    "Fixing \"%s\" failed with %u error%s\n",
		    filename,
		    nbErrors,
		    nbErrors == 1 ? "" : "s"
		);
	}
	return nbErrors;
}
