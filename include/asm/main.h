/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_MAIN_H
#define RGBDS_MAIN_H

#include <stdbool.h>
#include <stdint.h>

#include "helpers.h"

struct sOptions {
	char binary[2];
	char gbgfx[4];
	bool exportall;
	int32_t fillchar;
	bool haltnop;
	bool optimizeloads;
	bool verbose;
	bool warnings; /* True to enable warnings, false to disable them. */
};

extern char *tzNewMacro;
extern uint32_t ulNewMacroSize;
extern int32_t nGBGfxID;
extern int32_t nBinaryID;

extern struct sOptions DefaultOptions;
extern struct sOptions CurrentOptions;

extern FILE *dependfile;

void opt_Push(void);
void opt_Pop(void);
void opt_Parse(char *s);

/*
 * Used for errors that compromise the whole assembly process by affecting the
 * folliwing code, potencially making the assembler generate errors caused by
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

/*
 * Used to warn the user about problems that don't prevent the generation of
 * valid code.
 */
void warning(const char *fmt, ...);

#define YY_FATAL_ERROR fatalerror

#ifdef YYLMAX
#undef YYLMAX
#endif
#define YYLMAX 65536

#endif /* RGBDS_MAIN_H */
