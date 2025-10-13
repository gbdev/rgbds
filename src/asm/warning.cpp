// SPDX-License-Identifier: MIT

#include "asm/warning.hpp"

#include <functional>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostics.hpp"
#include "style.hpp"

#include "asm/fstack.hpp"
#include "asm/main.hpp"

// clang-format off: nested initializers
Diagnostics<WarningLevel, WarningID> warnings = {
    .metaWarnings = {
        {"all",                  LEVEL_ALL       },
        {"extra",                LEVEL_EXTRA     },
        {"everything",           LEVEL_EVERYTHING},
    },
    .warningFlags = {
        {"assert",               LEVEL_DEFAULT   },
        {"backwards-for",        LEVEL_ALL       },
        {"builtin-args",         LEVEL_ALL       },
        {"charmap-redef",        LEVEL_ALL       },
        {"div",                  LEVEL_EVERYTHING},
        {"empty-data-directive", LEVEL_ALL       },
        {"empty-macro-arg",      LEVEL_EXTRA     },
        {"empty-strrpl",         LEVEL_ALL       },
        {"export-undefined",     LEVEL_ALL       },
        {"large-constant",       LEVEL_DEFAULT   },
        {"macro-shift",          LEVEL_EXTRA     },
        {"nested-comment",       LEVEL_DEFAULT   },
        {"obsolete",             LEVEL_DEFAULT   },
        {"shift",                LEVEL_EVERYTHING},
        {"shift-amount",         LEVEL_EVERYTHING},
        {"unmatched-directive",  LEVEL_EXTRA     },
        {"unterminated-load",    LEVEL_EXTRA     },
        {"user",                 LEVEL_DEFAULT   },
        // Parametric warnings
        {"numeric-string",       LEVEL_EVERYTHING},
        {"numeric-string",       LEVEL_EVERYTHING},
        {"purge",                LEVEL_DEFAULT   },
        {"purge",                LEVEL_ALL       },
        {"truncation",           LEVEL_DEFAULT   },
        {"truncation",           LEVEL_EXTRA     },
        {"unmapped-char",        LEVEL_DEFAULT   },
        {"unmapped-char",        LEVEL_ALL       },
    },
    .paramWarnings = {
        {WARNING_NUMERIC_STRING_1, WARNING_NUMERIC_STRING_2, 1},
        {WARNING_PURGE_1,          WARNING_PURGE_2,          2},
        {WARNING_TRUNCATION_1,     WARNING_TRUNCATION_2,     1},
        {WARNING_UNMAPPED_CHAR_1,  WARNING_UNMAPPED_CHAR_2,  1},
    },
    .state = DiagnosticsState<WarningID>(),
    .nbErrors = 0,
};
// clang-format on

static void printDiag(
    char const *fmt,
    va_list args,
    char const *type,
    StyleColor color,
    char const *flagfmt,
    char const *flag
) {
	style_Set(stderr, color, true);
	fprintf(stderr, "%s: ", type);
	style_Reset(stderr);
	vfprintf(stderr, fmt, args);
	if (flagfmt) {
		style_Set(stderr, color, true);
		putc(' ', stderr);
		fprintf(stderr, flagfmt, flag);
	}
	putc('\n', stderr);

	fstk_TraceCurrent();
}

static void incrementErrors() {
	// This intentionally makes 0 act as "unlimited"
	warnings.incrementErrors();
	if (warnings.nbErrors == options.maxErrors) {
		style_Set(stderr, STYLE_RED, true);
		fprintf(
		    stderr,
		    "Assembly aborted after the maximum of %" PRIu64 " error%s!",
		    warnings.nbErrors,
		    warnings.nbErrors == 1 ? "" : "s"
		);
		style_Set(stderr, STYLE_RED, false);
		fputs(" (configure with '-X/--max-errors')\n", stderr);
		style_Reset(stderr);
		exit(1);
	}
}

void error(char const *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "error", STYLE_RED, nullptr, nullptr);
	va_end(args);

	incrementErrors();
}

void errorNoTrace(std::function<void()> callback) {
	style_Set(stderr, STYLE_RED, true);
	fputs("error: ", stderr);
	style_Reset(stderr);
	callback();

	incrementErrors();
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "FATAL", STYLE_RED, nullptr, nullptr);
	va_end(args);

	exit(1);
}

void requireZeroErrors() {
	if (warnings.nbErrors != 0) {
		style_Set(stderr, STYLE_RED, true);
		fprintf(
		    stderr,
		    "Assembly aborted with %" PRIu64 " error%s!\n",
		    warnings.nbErrors,
		    warnings.nbErrors == 1 ? "" : "s"
		);
		style_Reset(stderr);
		exit(1);
	}
}

void warning(WarningID id, char const *fmt, ...) {
	char const *flag = warnings.warningFlags[id].name;
	va_list args;

	va_start(args, fmt);

	switch (warnings.getWarningBehavior(id)) {
	case WarningBehavior::DISABLED:
		break;

	case WarningBehavior::ENABLED:
		printDiag(fmt, args, "warning", STYLE_YELLOW, "[-W%s]", flag);
		break;

	case WarningBehavior::ERROR:
		printDiag(fmt, args, "error", STYLE_RED, "[-Werror=%s]", flag);
		break;
	}

	va_end(args);
}
