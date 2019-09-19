/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/* Declarations that all modules use, as well as `main` and related */
#ifndef RGBDS_LINK_MAIN_H
#define RGBDS_LINK_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

/* Variables related to CLI options */
extern bool isDmgMode;
extern FILE *linkerScript;
extern FILE *mapFile;
extern FILE *symFile;
extern FILE *overlayFile;
extern FILE *outputFile;
extern uint8_t padValue;
extern bool is32kMode;
extern bool beVerbose;
extern bool isWRA0Mode;

/* Helper macro for printing verbose-mode messages */
#define verbosePrint(...)   do { \
					if (beVerbose) \
						fprintf(stderr, __VA_ARGS__); \
				} while (0)

#endif /* RGBDS_LINK_MAIN_H */
