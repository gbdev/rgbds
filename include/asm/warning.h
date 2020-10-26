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

enum WarningID {
	WARNING_ASSERT,		      /* Assertions */
	WARNING_BUILTIN_ARG,	      /* Invalid args to builtins */
	WARNING_CHARMAP_REDEF,        /* Charmap entry re-definition */
	WARNING_DIV,		      /* Division undefined behavior */
	WARNING_EMPTY_DATA_DIRECTIVE, /* `db`, `dw` or `dl` with no directive in ROM */
	WARNING_EMPTY_ENTRY,	      /* Empty entry in `db`, `dw` or `dl` */
	WARNING_LARGE_CONSTANT,	      /* Constants too large */
	WARNING_LONG_STR,	      /* String too long for internal buffers */
	WARNING_NESTED_COMMENT,	      /* Comment-start delimeter in a block comment */
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
_Noreturn void fatalerror(const char *fmt, ...);

/*
 * Used for errors that make it impossible to assemble correctly, but don't
 * affect the following code. The code will fail to assemble but the user will
 * get a list of all errors at the end, making it easier to fix all of them at
 * once.
 */
void error(const char *fmt, ...);

#endif
