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
#include <stdio.h>

#include "helpers.h"

struct sOptions {
	char binary[2];
	char gbgfx[4];
	int32_t fillchar;
};

extern char *tzNewMacro;
extern uint32_t ulNewMacroSize;
extern int32_t nGBGfxID;
extern int32_t nBinaryID;

extern uint32_t curOffset; /* Offset into the current section */

extern struct sOptions DefaultOptions;
extern struct sOptions CurrentOptions;
extern bool haltnop;
extern bool optimizeloads;
extern bool verbose;
extern bool warnings; /* True to enable warnings, false to disable them. */

extern FILE *dependfile;
extern char *tzTargetFileName;
extern bool oGeneratedMissingIncludes;
extern bool oFailedOnMissingInclude;
extern bool oGeneratePhonyDeps;

void opt_Push(void);
void opt_Pop(void);
void opt_Parse(char *s);

#define YY_FATAL_ERROR fatalerror

#ifdef YYLMAX
#undef YYLMAX
#endif
#define YYLMAX 65536

#endif /* RGBDS_MAIN_H */
