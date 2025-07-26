#include "fix/warning.hpp"

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
    .nbErrors = 0,
};
// clang-format on

uint32_t checkErrors(char const *filename) {
	if (warnings.nbErrors > 0) {
		fprintf(
		    stderr,
		    "Fixing \"%s\" failed with %" PRIu64 " error%s\n",
		    filename,
		    warnings.nbErrors,
		    warnings.nbErrors == 1 ? "" : "s"
		);
	}
	return warnings.nbErrors;
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

		warnings.incrementErrors();
		break;
	}
}
