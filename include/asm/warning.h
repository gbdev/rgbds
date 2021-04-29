/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef WARNING_H
#define WARNING_H

#include "helpers.h"

extern unsigned int nbErrors;

enum WarningState {
	WARNING_DEFAULT,
	WARNING_DISABLED,
	WARNING_ENABLED,
	WARNING_ERROR
};

enum WarningID {
	WARNING_ASSERT,		      /* Assertions */
	WARNING_BACKWARDS_FOR,	      /* `for` loop with backwards range */
	WARNING_BUILTIN_ARG,	      /* Invalid args to builtins */
	WARNING_CHARMAP_REDEF,        /* Charmap entry re-definition */
	WARNING_DIV,		      /* Division undefined behavior */
	WARNING_EMPTY_DATA_DIRECTIVE, /* `db`, `dw` or `dl` directive without data in ROM */
	WARNING_EMPTY_MACRO_ARG,      /* Empty macro argument */
	WARNING_EMPTY_STRRPL,	      /* Empty second argument in `STRRPL` */
	WARNING_LARGE_CONSTANT,	      /* Constants too large */
	WARNING_MACRO_SHIFT,	      /* Shift past available arguments in macro */
	WARNING_NESTED_COMMENT,	      /* Comment-start delimiter in a block comment */
	WARNING_OBSOLETE,	      /* Obsolete things */
	WARNING_SHIFT,		      /* Shifting undefined behavior */
	WARNING_SHIFT_AMOUNT,	      /* Strange shift amount */
	WARNING_TRUNCATION,	      /* Implicit truncation loses some bits */
	WARNING_USER,		      /* User warnings */

	NB_WARNINGS,

	/* Warnings past this point are "meta" warnings */
	WARNING_ALL = NB_WARNINGS,
	WARNING_EXTRA,
	WARNING_EVERYTHING,

	NB_WARNINGS_ALL
#define NB_META_WARNINGS (NB_WARNINGS_ALL - NB_WARNINGS)
};

extern enum WarningState warningStates[NB_WARNINGS];
extern bool warningsAreErrors;

void processWarningFlag(char const *flag);

/*
 * Used to warn the user about problems that don't prevent the generation of
 * valid code.
 */
void warning(enum WarningID id, const char *fmt, ...) format_(printf, 2, 3);

/*
 * Used for errors that compromise the whole assembly process by affecting the
 * following code, potencially making the assembler generate errors caused by
 * the first one and unrelated to the code that the assembler complains about.
 * It is also used when the assembler goes into an invalid state (for example,
 * when it fails to allocate memory).
 */
_Noreturn void fatalerror(const char *fmt, ...) format_(printf, 1, 2);

/*
 * Used for errors that make it impossible to assemble correctly, but don't
 * affect the following code. The code will fail to assemble but the user will
 * get a list of all errors at the end, making it easier to fix all of them at
 * once.
 */
void error(const char *fmt, ...) format_(printf, 1, 2);

#endif
