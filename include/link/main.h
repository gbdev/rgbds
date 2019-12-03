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
extern char const *linkerScriptName;
extern char const *mapFileName;
extern char const *symFileName;
extern char const *overlayFileName;
extern char const *outputFileName;
extern uint8_t padValue;
extern bool is32kMode;
extern bool beVerbose;
extern bool isWRA0Mode;

/* Helper macro for printing verbose-mode messages */
#define verbosePrint(...)   do { \
					if (beVerbose) \
						fprintf(stderr, __VA_ARGS__); \
				} while (0)

/**
 * Opens a file if specified, and aborts on error.
 * @param fileName The name of the file to open; if NULL, no file will be opened
 * @param mode The mode to open the file with
 * @return A pointer to a valid FILE structure, or NULL if fileName was NULL
 */
FILE * openFile(char const *fileName, char const *mode);

#define closeFile(file) do { \
				FILE *tmp = file; \
				if (tmp) \
					fclose(tmp); \
			} while (0)

#endif /* RGBDS_LINK_MAIN_H */
