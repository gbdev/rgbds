/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "link/object.h"
#include "link/symbol.h"
#include "link/section.h"
#include "link/assign.h"
#include "link/patch.h"
#include "link/output.h"

#include "extern/err.h"
#include "version.h"

bool isDmgMode;     /* -d */
FILE *linkerScript; /* -l */
FILE *mapFile;      /* -m */
FILE *symFile;      /* -n */
FILE *overlayFile;  /* -O */
FILE *outputFile;   /* -o */
uint8_t padValue;   /* -p */
bool is32kMode;     /* -t */
bool beVerbose;     /* -v */
bool isWRA0Mode;    /* -w */

/**
 * Prints the program's usage to stdout.
 */
static void printUsage(void)
{
	puts("usage: rgblink [-dtVvw] [-l linkerscript] [-m mapfile] [-n symfile] [-O overlay]\n"
	     "               [-o outfile] [-p pad_value] [-s symbol] file [...]");
}

/**
 * Helper function for `main`'s argument parsing.
 * For use with options which take a file name to operate on
 * If the file fails to be opened, an error message will be printed,
 * and the function `exit`s.
 * @param mode The mode to open the file in
 * @param failureMessage A format string that will be printed on failure.
 *                       A single (string) argument is given, the file name.
 * @return What `fopen` returned; this cannot be NULL.
 */
static FILE *openArgFile(char const *mode, char const *failureMessage)
{
	FILE *file = fopen(optarg, mode);

	if (!file)
		err(1, failureMessage, optarg);
	return file;
}

/**
 * Cleans up what has been done
 * Mostly here to please tools such as `valgrind` so actual errors can be seen
 */
static void cleanup(void)
{
	if (linkerScript)
		fclose(linkerScript);
	if (mapFile)
		fclose(mapFile);
	if (symFile)
		fclose(symFile);
	if (overlayFile)
		fclose(overlayFile);
	if (outputFile)
		fclose(outputFile);

	obj_Cleanup();
}

int main(int argc, char *argv[])
{
	char optionChar;
	char *endptr; /* For error checking with `strtol` */
	unsigned long value; /* For storing `strtoul`'s return value */

	/* Parse options */
	while ((optionChar = getopt(argc, argv, "dl:m:n:O:o:p:s:tVvw")) != -1) {
		switch (optionChar) {
		case 'd':
			isDmgMode = true;
			isWRA0Mode = true;
			break;
		case 'l':
			linkerScript = openArgFile("r", "Could not open linker script file \"%s\"");
			break;
		case 'm':
			mapFile = openArgFile("w", "Could not open map file \"%s\"");
			break;
		case 'n':
			symFile = openArgFile("w", "Could not open sym file \"%s\"");
			break;
		case 'O':
			overlayFile = openArgFile("r+b", "Could not open overlay file \"%s\"");
			break;
		case 'o':
			outputFile = openArgFile("wb", "Could not open output file \"%s\"");
			break;
		case 'p':
			value = strtoul(optarg, &endptr, 0);
			if (optarg[0] == '\0' || *endptr != '\0')
				errx(1, "Invalid argument for option 'p'");
			if (value > 0xFF)
				errx(1, "Argument for 'p' must be a byte (between 0 and 0xFF)");
			padValue = value;
			break;
		case 's':
			/* FIXME: nobody knows what this does, figure it out */
			(void)optarg;
			warnx("Nobody has any idea what `-s` does");
			break;
		case 't':
			is32kMode = true;
			break;
		case 'V':
			printf("rgblink %s\n", get_package_version_string());
			exit(0);
		case 'v':
			beVerbose = true;
			break;
		case 'w':
			isWRA0Mode = true;
			break;
		default:
			printUsage();
			exit(1);
		}
	}

	int curArgIndex = optind;

	/* If no input files were specified, the user must have screwed up */
	if (curArgIndex == argc) {
		fprintf(stderr, "No input files");
		printUsage();
		exit(1);
	}

	/* Patch the size array depending on command-line options */
	if (is32kMode)
		maxsize[SECTTYPE_ROM0] = 0x8000;
	if (isWRA0Mode)
		maxsize[SECTTYPE_WRAM0] = 0x2000;

	/* Patch the bank ranges array depending on command-line options */
	if (isDmgMode)
		bankranges[SECTTYPE_VRAM][1] = BANK_MIN_VRAM;

	/* Read all object files first, */
	while (curArgIndex < argc)
		obj_ReadFile(argv[curArgIndex++]);

	/* then process them, */
	obj_DoSanityChecks();
	assign_AssignSections();
	assign_Cleanup();

	/* and finally output the result. */
	patch_ApplyPatches();
	out_WriteFiles();

	/* Do cleanup before quitting, though. */
	cleanup();
}
