/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2021, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_MAIN_H
#define RGBDS_MAIN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "helpers.h"

extern bool haltnop;
extern bool optimizeloads;
extern bool verbose;
extern bool warnings; /* True to enable warnings, false to disable them. */

extern FILE *dependfile;
extern char *tzTargetFileName;
extern bool oGeneratedMissingIncludes;
extern bool oFailedOnMissingInclude;
extern bool oGeneratePhonyDeps;

/* TODO: are these really needed? */
#define YY_FATAL_ERROR fatalerror

#ifdef YYLMAX
#undef YYLMAX
#endif
#define YYLMAX 65536

#endif /* RGBDS_MAIN_H */
