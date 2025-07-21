// SPDX-License-Identifier: MIT

#include "asm/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics.hpp"
#include "helpers.hpp"
#include "itertools.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp"

unsigned int nbErrors = 0;

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
        {"large-constant",       LEVEL_ALL       },
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
        {WARNING_PURGE_1,          WARNING_PURGE_2,          1},
        {WARNING_TRUNCATION_1,     WARNING_TRUNCATION_2,     2},
        {WARNING_UNMAPPED_CHAR_1,  WARNING_UNMAPPED_CHAR_2,  1},
    },
    .state = DiagnosticsState<WarningID>(),
};
// clang-format on

static void printDiag(
    char const *fmt, va_list args, char const *type, char const *flagfmt, char const *flag
) {
	fputs(type, stderr);
	fputs(": ", stderr);
	if (fstk_DumpCurrent()) {
		fprintf(stderr, flagfmt, flag);
		fputs("\n    ", stderr);
	}
	vfprintf(stderr, fmt, args);
	putc('\n', stderr);
	lexer_DumpStringExpansions();
}

static void incrementErrors() {
	// This intentionally makes 0 act as "unlimited" (or at least "limited to sizeof(unsigned)")
	if (++nbErrors == options.maxErrors) {
		fprintf(
		    stderr,
		    "Assembly aborted after the maximum of %u error%s! (configure with "
		    "'-X/--max-errors')\n",
		    nbErrors,
		    nbErrors == 1 ? "" : "s"
		);
		exit(1);
	}
}

void error(char const *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "error", ":", nullptr);
	va_end(args);

	incrementErrors();
}

void error(std::function<void()> callback) {
	fputs("error: ", stderr);
	if (fstk_DumpCurrent()) {
		fputs(":\n    ", stderr);
	}
	callback();
	putc('\n', stderr);
	lexer_DumpStringExpansions();

	incrementErrors();
}

[[noreturn]]
void fatal(char const *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "FATAL", ":", nullptr);
	va_end(args);

	exit(1);
}

void warning(WarningID id, char const *fmt, ...) {
	char const *flag = warnings.warningFlags[id].name;
	va_list args;

	va_start(args, fmt);

	switch (warnings.getWarningBehavior(id)) {
	case WarningBehavior::DISABLED:
		break;

	case WarningBehavior::ENABLED:
		printDiag(fmt, args, "warning", ": [-W%s]", flag);
		break;

	case WarningBehavior::ERROR:
		printDiag(fmt, args, "error", ": [-Werror=%s]", flag);
		break;
	}

	va_end(args);
}
