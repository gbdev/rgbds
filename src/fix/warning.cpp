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
        {"sgb",        LEVEL_DEFAULT   },
        {"truncation", LEVEL_DEFAULT   },
    },
    .paramWarnings = {},
    .state = DiagnosticsState<WarningID>(),
};
// clang-format on

void resetErrors() {
	nbErrors = 0;
}

bool anyErrors() {
	return nbErrors > 0;
}

uint32_t checkErrors(char const *filename) {
	if (anyErrors()) {
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

static void incrementErrors() {
	if (nbErrors != UINT32_MAX) {
		++nbErrors;
	}
}

void error(char const *fmt, ...) {
	va_list ap;
	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	incrementErrors();
}

void fatal(char const *fmt, ...) {
	va_list ap;
	fputs("FATAL: ", stderr);
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

		incrementErrors();
		break;
	}
}
