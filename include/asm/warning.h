/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef WARNING_H
#define WARNING_H

extern unsigned int nbErrors;

enum WarningID {
	WARNING_BUILTIN_ARG,
	WARNING_DIV,
	WARNING_EMPTY_ENTRY,
	WARNING_LARGE_CONSTANT,
	WARNING_LONG_STR,
	WARNING_OBSOLETE,
	WARNING_SHIFT,
	WARNING_USER,
	WARNING_SHIFT_AMOUNT,
	WARNING_TRUNCATION,

	NB_WARNINGS,

	/* Warnings past this point are "meta" warnings */
	WARNING_ALL = NB_WARNINGS,
	WARNING_EXTRA,
	WARNING_EVERYTHING,

	NB_WARNINGS_ALL
#define NB_META_WARNINGS (NB_WARNINGS_ALL - NB_WARNINGS)
};

void processWarningFlag(char const *flag);

/*
 * Used to warn the user about problems that don't prevent the generation of
 * valid code.
 */
void warning(enum WarningID id, const char *fmt, ...);

/*
 * Used for errors that compromise the whole assembly process by affecting the
 * following code, potencially making the assembler generate errors caused by
 * the first one and unrelated to the code that the assembler complains about.
 * It is also used when the assembler goes into an invalid state (for example,
 * when it fails to allocate memory).
 */
noreturn_ void fatalerror(const char *fmt, ...);

/*
 * Used for errors that make it impossible to assemble correctly, but don't
 * affect the following code. The code will fail to assemble but the user will
 * get a list of all errors at the end, making it easier to fix all of them at
 * once.
 */
void yyerror(const char *fmt, ...);

#endif
