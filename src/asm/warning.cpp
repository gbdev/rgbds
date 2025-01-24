/* SPDX-License-Identifier: MIT */

#include "asm/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.hpp"
#include "helpers.hpp"
#include "itertools.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp"

unsigned int nbErrors = 0;
unsigned int maxErrors = 0;

Diagnostics warningStates;
bool warningsAreErrors;

enum WarningLevel {
	LEVEL_DEFAULT,    // Warnings that are enabled by default
	LEVEL_ALL,        // Warnings that probably indicate an error
	LEVEL_EXTRA,      // Warnings that are less likely to indicate an error
	LEVEL_EVERYTHING, // Literally every warning
};

struct WarningFlag {
	char const *name;
	WarningLevel level;
};

static const WarningFlag metaWarnings[] = {
    {"all",        LEVEL_ALL       },
    {"extra",      LEVEL_EXTRA     },
    {"everything", LEVEL_EVERYTHING},
};

static const WarningFlag warningFlags[NB_WARNINGS] = {
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
};

static const struct {
	WarningID firstID;
	WarningID lastID;
	uint8_t defaultLevel;
} paramWarnings[] = {
    {WARNING_NUMERIC_STRING_1, WARNING_NUMERIC_STRING_2, 1},
    {WARNING_PURGE_1,          WARNING_PURGE_2,          1},
    {WARNING_TRUNCATION_1,     WARNING_TRUNCATION_2,     2},
    {WARNING_UNMAPPED_CHAR_1,  WARNING_UNMAPPED_CHAR_2,  1},
};

enum WarningBehavior { DISABLED, ENABLED, ERROR };

static WarningBehavior getWarningBehavior(WarningID id) {
	// Check if warnings are globally disabled
	if (!warnings)
		return WarningBehavior::DISABLED;

	// Get the state of this warning flag
	WarningState const &flagState = warningStates.flagStates[id];
	WarningState const &metaState = warningStates.metaStates[id];

	// If subsequent checks determine that the warning flag is enabled, this checks whether it has
	// -Werror without -Wno-error=<flag> or -Wno-error=<meta>, which makes it into an error
	bool warningIsError = warningsAreErrors && flagState.error != WARNING_DISABLED
	                      && metaState.error != WARNING_DISABLED;
	WarningBehavior enabledBehavior =
	    warningIsError ? WarningBehavior::ERROR : WarningBehavior::ENABLED;

	// First, check the state of the specific warning flag
	if (flagState.state == WARNING_DISABLED) // -Wno-<flag>
		return WarningBehavior::DISABLED;
	if (flagState.error == WARNING_ENABLED) // -Werror=<flag>
		return WarningBehavior::ERROR;
	if (flagState.state == WARNING_ENABLED) // -W<flag>
		return enabledBehavior;

	// If no flag is specified, check the state of the "meta" flags that affect this warning flag
	if (metaState.state == WARNING_DISABLED) // -Wno-<meta>
		return WarningBehavior::DISABLED;
	if (metaState.error == WARNING_ENABLED) // -Werror=<meta>
		return WarningBehavior::ERROR;
	if (metaState.state == WARNING_ENABLED) // -W<meta>
		return enabledBehavior;

	// If no meta flag is specified, check the default state of this warning flag
	if (warningFlags[id].level == LEVEL_DEFAULT) // enabled by default
		return enabledBehavior;

	// No flag enables this warning, explicitly or implicitly
	return WarningBehavior::DISABLED;
}

void WarningState::update(WarningState other) {
	if (other.state != WARNING_DEFAULT)
		state = other.state;
	if (other.error != WARNING_DEFAULT)
		error = other.error;
}

void processWarningFlag(char const *flag) {
	std::string rootFlag = flag;

	// Check for `-Werror` or `-Wno-error` to return early
	if (rootFlag == "error") {
		// `-Werror` promotes warnings to errors
		warningsAreErrors = true;
		return;
	} else if (rootFlag == "no-error") {
		// `-Wno-error` disables promotion of warnings to errors
		warningsAreErrors = false;
		return;
	}

	// Check for prefixes that affect what the flag does
	WarningState state;
	if (rootFlag.starts_with("error=")) {
		// `-Werror=<flag>` enables the flag as an error
		state = {.state = WARNING_ENABLED, .error = WARNING_ENABLED};
		rootFlag.erase(0, literal_strlen("error="));
	} else if (rootFlag.starts_with("no-error=")) {
		// `-Wno-error=<flag>` prevents the flag from being an error,
		// without affecting whether it is enabled
		state = {.state = WARNING_DEFAULT, .error = WARNING_DISABLED};
		rootFlag.erase(0, literal_strlen("no-error="));
	} else if (rootFlag.starts_with("no-")) {
		// `-Wno-<flag>` disables the flag
		state = {.state = WARNING_DISABLED, .error = WARNING_DEFAULT};
		rootFlag.erase(0, literal_strlen("no-"));
	} else {
		// `-W<flag>` enables the flag
		state = {.state = WARNING_ENABLED, .error = WARNING_DEFAULT};
	}

	// Check for an `=` parameter to process as a parametric warning
	// `-Wno-<flag>` and `-Wno-error=<flag>` negation cannot have an `=` parameter, but without a
	// parameter, the 0 value will apply to all levels of a parametric warning
	uint8_t param = 0;
	bool hasParam = false;
	if (state.state == WARNING_ENABLED) {
		// First, check if there is an "equals" sign followed by a decimal number
		// Ignore an equal sign at the very end of the string
		if (auto equals = rootFlag.find('=');
		    equals != rootFlag.npos && equals != rootFlag.size() - 1) {
			hasParam = true;

			// Is the rest of the string a decimal number?
			// We want to avoid `strtoul`'s whitespace and sign, so we parse manually
			char const *ptr = rootFlag.c_str() + equals + 1;
			bool warned = false;

			// The `if`'s condition above ensures that this will run at least once
			do {
				// If we don't have a digit, bail
				if (*ptr < '0' || *ptr > '9')
					break;
				// Avoid overflowing!
				if (param > UINT8_MAX - (*ptr - '0')) {
					if (!warned)
						warnx("Invalid warning flag \"%s\": capping parameter at 255", flag);
					warned = true; // Only warn once, cap always
					param = 255;
					continue;
				}
				param = param * 10 + (*ptr - '0');

				ptr++;
			} while (*ptr);

			// If we reached the end of the string, truncate it at the '='
			if (*ptr == '\0') {
				rootFlag.resize(equals);
				// `-W<flag>=0` is equivalent to `-Wno-<flag>`
				if (param == 0)
					state.state = WARNING_DISABLED;
			}
		}
	}

	// Try to match the flag against a parametric warning
	// If there was an equals sign, it will have set `param`; if not, `param` will be 0, which
	// applies to all levels
	for (auto const &paramWarning : paramWarnings) {
		WarningID baseID = paramWarning.firstID;
		uint8_t maxParam = paramWarning.lastID - baseID + 1;
		assume(paramWarning.defaultLevel <= maxParam);

		if (rootFlag == warningFlags[baseID].name) { // Match!
			if (rootFlag == "numeric-string")
				warning(WARNING_OBSOLETE, "Warning flag \"numeric-string\" is deprecated\n");

			// If making the warning an error but param is 0, set to the maximum
			// This accommodates `-Werror=<flag>`, but also `-Werror=<flag>=0`, which is
			// thus filtered out by the caller.
			// A param of 0 makes sense for disabling everything, but neither for
			// enabling nor "erroring". Use the default for those.
			if (param == 0) {
				param = paramWarning.defaultLevel;
			} else if (param > maxParam) {
				if (param != 255) // Don't warn if already capped
					warnx(
					    "Invalid parameter %" PRIu8
					    " for warning flag \"%s\"; capping at maximum %" PRIu8,
					    param,
					    rootFlag.c_str(),
					    maxParam
					);
				param = maxParam;
			}

			// Set the first <param> to enabled/error, and disable the rest
			for (uint8_t ofs = 0; ofs < maxParam; ofs++) {
				WarningState &warning = warningStates.flagStates[baseID + ofs];
				if (ofs < param)
					warning.update(state);
				else
					warning.state = WARNING_DISABLED;
			}
			return;
		}
	}

	// Try to match against a non-parametric warning, unless there was an equals sign
	if (!hasParam) {
		// Try to match against a "meta" warning
		for (WarningFlag const &metaWarning : metaWarnings) {
			if (rootFlag == metaWarning.name) {
				// Set each of the warning flags that meets this level
				for (WarningID id : EnumSeq(NB_WARNINGS)) {
					if (metaWarning.level >= warningFlags[id].level)
						warningStates.metaStates[id].update(state);
				}
				return;
			}
		}

		// Try to match the flag against a "normal" flag
		for (WarningID id : EnumSeq(NB_PLAIN_WARNINGS)) {
			if (rootFlag == warningFlags[id].name) {
				warningStates.flagStates[id].update(state);
				return;
			}
		}
	}

	warnx("Unknown warning flag \"%s\"", flag);
}

void printDiag(
    char const *fmt, va_list args, char const *type, char const *flagfmt, char const *flag
) {
	fputs(type, stderr);
	fputs(": ", stderr);
	fstk_DumpCurrent();
	fprintf(stderr, flagfmt, flag);
	fputs("\n    ", stderr);
	vfprintf(stderr, fmt, args);
	lexer_DumpStringExpansions();
}

void error(char const *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "error", ":", nullptr);
	va_end(args);

	// This intentionally makes 0 act as "unlimited" (or at least "limited to sizeof(unsigned)")
	nbErrors++;
	if (nbErrors == maxErrors)
		errx(
		    "The maximum of %u error%s was reached (configure with \"-X/--max-errors\"); assembly "
		    "aborted!",
		    maxErrors,
		    maxErrors == 1 ? "" : "s"
		);
}

[[noreturn]] void fatalerror(char const *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	printDiag(fmt, args, "FATAL", ":", nullptr);
	va_end(args);

	exit(1);
}

void warning(WarningID id, char const *fmt, ...) {
	char const *flag = warningFlags[id].name;
	va_list args;

	va_start(args, fmt);

	switch (getWarningBehavior(id)) {
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
