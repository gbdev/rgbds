/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_WARNING_HPP
#define RGBDS_ASM_WARNING_HPP

extern unsigned int nbErrors, maxErrors;

enum WarningID {
	WARNING_ASSERT,               // Assertions
	WARNING_BACKWARDS_FOR,        // `for` loop with backwards range
	WARNING_BUILTIN_ARG,          // Invalid args to builtins
	WARNING_CHARMAP_REDEF,        // Charmap entry re-definition
	WARNING_DIV,                  // Division undefined behavior
	WARNING_EMPTY_DATA_DIRECTIVE, // `db`, `dw` or `dl` directive without data in ROM
	WARNING_EMPTY_MACRO_ARG,      // Empty macro argument
	WARNING_EMPTY_STRRPL,         // Empty second argument in `STRRPL`
	WARNING_LARGE_CONSTANT,       // Constants too large
	WARNING_MACRO_SHIFT,          // Shift past available arguments in macro
	WARNING_NESTED_COMMENT,       // Comment-start delimiter in a block comment
	WARNING_OBSOLETE,             // Obsolete things
	WARNING_SHIFT,                // Shifting undefined behavior
	WARNING_SHIFT_AMOUNT,         // Strange shift amount
	WARNING_USER,                 // User warnings

	NB_PLAIN_WARNINGS,

	// Warnings past this point are "parametric" warnings, only mapping to a single flag
	// Treating string as number may lose some bits
	WARNING_NUMERIC_STRING_1 = NB_PLAIN_WARNINGS,
	WARNING_NUMERIC_STRING_2,
	// Purging an exported symbol or label
	WARNING_PURGE_1,
	WARNING_PURGE_2,
	// Implicit truncation loses some bits
	WARNING_TRUNCATION_1,
	WARNING_TRUNCATION_2,
	// Character without charmap entry
	WARNING_UNMAPPED_CHAR_1,
	WARNING_UNMAPPED_CHAR_2,

	NB_WARNINGS,
};

enum WarningAbled { WARNING_DEFAULT, WARNING_ENABLED, WARNING_DISABLED };

struct WarningState {
	WarningAbled state;
	WarningAbled error;

	void update(WarningState other);
};

struct Diagnostics {
	WarningState flagStates[NB_WARNINGS];
	WarningState metaStates[NB_WARNINGS];
};

extern Diagnostics warningStates;
extern bool warningsAreErrors;

void processWarningFlag(char const *flag);

/*
 * Used to warn the user about problems that don't prevent the generation of
 * valid code.
 */
[[gnu::format(printf, 2, 3)]] void warning(WarningID id, char const *fmt, ...);

/*
 * Used for errors that compromise the whole assembly process by affecting the
 * following code, potencially making the assembler generate errors caused by
 * the first one and unrelated to the code that the assembler complains about.
 * It is also used when the assembler goes into an invalid state (for example,
 * when it fails to allocate memory).
 */
[[gnu::format(printf, 1, 2), noreturn]] void fatalerror(char const *fmt, ...);

/*
 * Used for errors that make it impossible to assemble correctly, but don't
 * affect the following code. The code will fail to assemble but the user will
 * get a list of all errors at the end, making it easier to fix all of them at
 * once.
 */
[[gnu::format(printf, 1, 2)]] void error(char const *fmt, ...);

#endif // RGBDS_ASM_WARNING_HPP
