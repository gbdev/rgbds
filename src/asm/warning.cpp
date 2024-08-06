/* SPDX-License-Identifier: MIT */

#include "asm/warning.hpp"

#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.hpp"
#include "helpers.hpp" // QUOTEDSTRLEN
#include "itertools.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp"

unsigned int nbErrors = 0;
unsigned int maxErrors = 0;

static WarningState const defaultWarnings[ARRAY_SIZE(warningStates)] = {
    WARNING_ENABLED,  // WARNING_ASSERT
    WARNING_DISABLED, // WARNING_BACKWARDS_FOR
    WARNING_DISABLED, // WARNING_BUILTIN_ARG
    WARNING_DISABLED, // WARNING_CHARMAP_REDEF
    WARNING_DISABLED, // WARNING_DIV
    WARNING_DISABLED, // WARNING_EMPTY_DATA_DIRECTIVE
    WARNING_DISABLED, // WARNING_EMPTY_MACRO_ARG
    WARNING_DISABLED, // WARNING_EMPTY_STRRPL
    WARNING_DISABLED, // WARNING_LARGE_CONSTANT
    WARNING_DISABLED, // WARNING_MACRO_SHIFT
    WARNING_ENABLED,  // WARNING_NESTED_COMMENT
    WARNING_ENABLED,  // WARNING_OBSOLETE
    WARNING_DISABLED, // WARNING_SHIFT
    WARNING_DISABLED, // WARNING_SHIFT_AMOUNT
    WARNING_ENABLED,  // WARNING_USER

    WARNING_DISABLED, // WARNING_NUMERIC_STRING_1
    WARNING_DISABLED, // WARNING_NUMERIC_STRING_2
    WARNING_ENABLED,  // WARNING_TRUNCATION_1
    WARNING_DISABLED, // WARNING_TRUNCATION_2
    WARNING_ENABLED,  // WARNING_PURGE_1
    WARNING_DISABLED, // WARNING_PURGE_2
    WARNING_ENABLED,  // WARNING_UNMAPPED_CHAR_1
    WARNING_DISABLED, // WARNING_UNMAPPED_CHAR_2
};

WarningState warningStates[ARRAY_SIZE(warningStates)];

bool warningsAreErrors; // Set if `-Werror` was specified

static WarningState warningState(WarningID id) {
	// Check if warnings are globally disabled
	if (!warnings)
		return WARNING_DISABLED;

	// Get the actual state
	WarningState state = warningStates[id];

	if (state == WARNING_DEFAULT)
		// The state isn't set, grab its default state
		state = defaultWarnings[id];

	if (warningsAreErrors && state == WARNING_ENABLED)
		state = WARNING_ERROR;

	return state;
}

static char const * const warningFlags[NB_WARNINGS] = {
    "assert",
    "backwards-for",
    "builtin-args",
    "charmap-redef",
    "div",
    "empty-data-directive",
    "empty-macro-arg",
    "empty-strrpl",
    "large-constant",
    "macro-shift",
    "nested-comment",
    "obsolete",
    "shift",
    "shift-amount",
    "user",

    // Parametric warnings
    "numeric-string",
    "numeric-string",
    "purge",
    "purge",
    "truncation",
    "truncation",
    "unmapped-char",
    "unmapped-char",

    // Meta warnings
    "all",
    "extra",
    "everything", // Especially useful for testing
};

static const struct {
	char const *name;
	uint8_t nbLevels;
	uint8_t defaultLevel;
} paramWarnings[] = {
    {"numeric-string", 2, 1},
    {"purge",          2, 1},
    {"truncation",     2, 2},
    {"unmapped-char",  2, 1},
};

static bool tryProcessParamWarning(char const *flag, uint8_t param, WarningState state) {
	WarningID baseID = PARAM_WARNINGS_START;

	for (size_t i = 0; i < ARRAY_SIZE(paramWarnings); i++) {
		uint8_t maxParam = paramWarnings[i].nbLevels;

		if (!strcmp(flag, paramWarnings[i].name)) { // Match!
			if (!strcmp(flag, "numeric-string"))
				warning(WARNING_OBSOLETE, "Warning flag \"numeric-string\" is deprecated\n");

			// If making the warning an error but param is 0, set to the maximum
			// This accommodates `-Werror=flag`, but also `-Werror=flag=0`, which is
			// thus filtered out by the caller.
			// A param of 0 makes sense for disabling everything, but neither for
			// enabling nor "erroring". Use the default for those.
			if (param == 0 && state != WARNING_DISABLED) {
				param = paramWarnings[i].defaultLevel;
			} else if (param > maxParam) {
				if (param != 255) // Don't  warn if already capped
					warnx(
					    "Got parameter %" PRIu8
					    " for warning flag \"%s\", but the maximum is %" PRIu8 "; capping.\n",
					    param,
					    flag,
					    maxParam
					);
				param = maxParam;
			}

			// Set the first <param> to enabled/error, and disable the rest
			for (uint8_t ofs = 0; ofs < maxParam; ofs++) {
				warningStates[baseID + ofs] = ofs < param ? state : WARNING_DISABLED;
			}
			return true;
		}

		baseID = (WarningID)(baseID + maxParam);
	}
	return false;
}

enum MetaWarningCommand { META_WARNING_DONE = NB_WARNINGS };

// Warnings that probably indicate an error
static uint8_t const _wallCommands[] = {
    WARNING_BACKWARDS_FOR,
    WARNING_BUILTIN_ARG,
    WARNING_CHARMAP_REDEF,
    WARNING_EMPTY_DATA_DIRECTIVE,
    WARNING_EMPTY_STRRPL,
    WARNING_LARGE_CONSTANT,
    WARNING_NESTED_COMMENT,
    WARNING_OBSOLETE,
    WARNING_PURGE_1,
    WARNING_PURGE_2,
    WARNING_UNMAPPED_CHAR_1,
    META_WARNING_DONE,
};

// Warnings that are less likely to indicate an error
static uint8_t const _wextraCommands[] = {
    WARNING_EMPTY_MACRO_ARG,
    WARNING_MACRO_SHIFT,
    WARNING_NESTED_COMMENT,
    WARNING_OBSOLETE,
    WARNING_PURGE_1,
    WARNING_PURGE_2,
    WARNING_TRUNCATION_1,
    WARNING_TRUNCATION_2,
    WARNING_UNMAPPED_CHAR_1,
    WARNING_UNMAPPED_CHAR_2,
    META_WARNING_DONE,
};

// Literally everything. Notably useful for testing
static uint8_t const _weverythingCommands[] = {
    WARNING_BACKWARDS_FOR,
    WARNING_BUILTIN_ARG,
    WARNING_DIV,
    WARNING_EMPTY_DATA_DIRECTIVE,
    WARNING_EMPTY_MACRO_ARG,
    WARNING_EMPTY_STRRPL,
    WARNING_LARGE_CONSTANT,
    WARNING_MACRO_SHIFT,
    WARNING_NESTED_COMMENT,
    WARNING_OBSOLETE,
    WARNING_PURGE_1,
    WARNING_PURGE_2,
    WARNING_SHIFT,
    WARNING_SHIFT_AMOUNT,
    WARNING_TRUNCATION_1,
    WARNING_TRUNCATION_2,
    WARNING_UNMAPPED_CHAR_1,
    WARNING_UNMAPPED_CHAR_2,
    // WARNING_USER,
    META_WARNING_DONE,
};

static uint8_t const *metaWarningCommands[NB_META_WARNINGS] = {
    _wallCommands,
    _wextraCommands,
    _weverythingCommands,
};

void processWarningFlag(char const *flag) {
	static bool setError = false;

	// First, try to match against a "meta" warning
	for (WarningID id : EnumSeq(META_WARNINGS_START, NB_WARNINGS)) {
		// TODO: improve the matching performance?
		if (!strcmp(flag, warningFlags[id])) {
			// We got a match!
			if (setError)
				errx("Cannot make meta warning \"%s\" into an error", flag);

			for (uint8_t const *ptr = metaWarningCommands[id - META_WARNINGS_START];
			     *ptr != META_WARNING_DONE;
			     ptr++) {
				// Warning flag, set without override
				if (warningStates[*ptr] == WARNING_DEFAULT)
					warningStates[*ptr] = WARNING_ENABLED;
			}

			return;
		}
	}

	// If it's not a meta warning, specially check against `-Werror`
	if (!strncmp(flag, "error", QUOTEDSTRLEN("error"))) {
		char const *errorFlag = flag + QUOTEDSTRLEN("error");

		switch (*errorFlag) {
		case '\0':
			// `-Werror`
			warningsAreErrors = true;
			return;

		case '=':
			// `-Werror=XXX`
			setError = true;
			processWarningFlag(errorFlag + 1); // Skip the `=`
			setError = false;
			return;

			// Otherwise, allow parsing as another flag
		}
	}

	// Well, it's either a normal warning or a mistake

	WarningState state = setError ? WARNING_ERROR
	                     // Not an error, then check if this is a negation
	                     : strncmp(flag, "no-", QUOTEDSTRLEN("no-")) ? WARNING_ENABLED
	                                                                 : WARNING_DISABLED;
	char const *rootFlag = state == WARNING_DISABLED ? flag + QUOTEDSTRLEN("no-") : flag;

	// Is this a "parametric" warning?
	if (state != WARNING_DISABLED) { // The `no-` form cannot be parametrized
		// First, check if there is an "equals" sign followed by a decimal number
		char const *equals = strchr(rootFlag, '=');

		if (equals && equals[1] != '\0') { // Ignore an equal sign at the very end as well
			// Is the rest of the string a decimal number?
			// We want to avoid `strtoul`'s whitespace and sign, so we parse manually
			uint8_t param = 0;
			char const *ptr = equals + 1;
			bool warned = false;

			// The `if`'s condition above ensures that this will run at least once
			do {
				// If we don't have a digit, bail
				if (*ptr < '0' || *ptr > '9')
					break;
				// Avoid overflowing!
				if (param > UINT8_MAX - (*ptr - '0')) {
					if (!warned)
						warnx("Invalid warning flag \"%s\": capping parameter at 255\n", flag);
					warned = true; // Only warn once, cap always
					param = 255;
					continue;
				}
				param = param * 10 + (*ptr - '0');

				ptr++;
			} while (*ptr);

			// If we managed to the end of the string, check that the warning indeed
			// accepts a parameter
			if (*ptr == '\0') {
				if (setError && param == 0) {
					warnx("Ignoring nonsensical warning flag \"%s\"\n", flag);
					return;
				}

				std::string truncFlag = rootFlag;

				truncFlag.resize(equals - rootFlag); // Truncate the param at the '='
				if (tryProcessParamWarning(
				        truncFlag.c_str(), param, param == 0 ? WARNING_DISABLED : state
				    ))
					return;
			}
		}
	}

	// Try to match the flag against a "normal" flag
	for (WarningID id : EnumSeq(NB_PLAIN_WARNINGS)) {
		if (!strcmp(rootFlag, warningFlags[id])) {
			// We got a match!
			warningStates[id] = state;
			return;
		}
	}

	// Lastly, this might be a "parametric" warning without an equals sign
	// If it is, treat the param as 1 if enabling, or 0 if disabling
	if (tryProcessParamWarning(rootFlag, 0, state))
		return;

	warnx("Unknown warning `%s`", flag);
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
	char const *flag = warningFlags[id];
	va_list args;

	va_start(args, fmt);

	switch (warningState(id)) {
	case WARNING_DISABLED:
		break;

	case WARNING_ENABLED:
		printDiag(fmt, args, "warning", ": [-W%s]", flag);
		break;

	case WARNING_ERROR:
		printDiag(fmt, args, "error", ": [-Werror=%s]", flag);
		break;

	case WARNING_DEFAULT:
		unreachable_();
		// Not reached
	}

	va_end(args);
}
