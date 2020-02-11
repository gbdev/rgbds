/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <sys/types.h>
#include <sys/stat.h>
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
#include "extern/getopt.h"
#include "version.h"

bool isDmgMode;               /* -d */
char const *linkerScriptName; /* -l */
char const *mapFileName;      /* -m */
char const *symFileName;      /* -n */
char const *overlayFileName;  /* -O */
char const *outputFileName;   /* -o */
uint8_t padValue;             /* -p */
bool is32kMode;               /* -t */
bool beVerbose;               /* -v */
bool isWRA0Mode;              /* -w */

FILE *openFile(char const *fileName, char const *mode)
{
	if (!fileName)
		return NULL;

	FILE *file = fopen(fileName, mode);

	if (!file)
		err(1, "Could not open file \"%s\"", fileName);

	return file;
}

/* Short options */
static char const *optstring = "dl:m:n:O:o:p:s:tVvw";

/*
 * Equivalent long options
 * Please keep in the same order as short opts
 *
 * Also, make sure long opts don't create ambiguity:
 * A long opt's name should start with the same letter as its short opt,
 * except if it doesn't create any ambiguity (`verbose` versus `version`).
 * This is because long opt matching, even to a single char, is prioritized
 * over short opt matching
 */
static struct option const longopts[] = {
	{ "dmg",          no_argument,       NULL, 'd' },
	{ "linkerscript", required_argument, NULL, 'l' },
	{ "map",          required_argument, NULL, 'm' },
	{ "sym",          required_argument, NULL, 'n' },
	{ "overlay",      required_argument, NULL, 'O' },
	{ "output",       required_argument, NULL, 'o' },
	{ "pad",          required_argument, NULL, 'p' },
	{ "smart",        required_argument, NULL, 's' },
	{ "tiny",         no_argument,       NULL, 't' },
	{ "version",      no_argument,       NULL, 'V' },
	{ "verbose",      no_argument,       NULL, 'v' },
	{ "wramx",        no_argument,       NULL, 'w' },
	{ NULL,           no_argument,       NULL, 0   }
};

/**
 * Prints the program's usage to stdout.
 */
static void printUsage(void)
{
	fputs(
"Usage: rgblink [-dtVvw] [-l script] [-m map_file] [-n sym_file]\n"
"               [-O overlay_file] [-o out_file] [-p pad_value] [-s symbol]\n"
"               <file> ...\n"
"Useful options:\n"
"    -l, --linkerscript <path>  set the input linker script\n"
"    -m, --map <path>           set the output map file\n"
"    -n, --sym <path>           set the output symbol list file\n"
"    -o, --output <path>        set the output file\n"
"    -p, --pad <value>          set the value to pad between sections with\n"
"    -V, --version              print RGBLINK version and exits\n"
"\n"
"For help, use `man rgblink' or go to https://rednex.github.io/rgbds/\n",
	      stderr);
}

/**
 * Cleans up what has been done
 * Mostly here to please tools such as `valgrind` so actual errors can be seen
 */
static void cleanup(void)
{
	obj_Cleanup();
}

int main(int argc, char *argv[])
{
	int optionChar;
	char *endptr; /* For error checking with `strtol` */
	unsigned long value; /* For storing `strtoul`'s return value */

	/* Parse options */
	while ((optionChar = musl_getopt_long_only(argc, argv, optstring,
						   longopts, NULL)) != -1) {
		switch (optionChar) {
		case 'd':
			isDmgMode = true;
			isWRA0Mode = true;
			break;
		case 'l':
			linkerScriptName = optarg;
			break;
		case 'm':
			mapFileName = optarg;
			break;
		case 'n':
			symFileName = optarg;
			break;
		case 'O':
			overlayFileName = optarg;
			break;
		case 'o':
			outputFileName = optarg;
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
		fputs("FATAL: no input files\n", stderr);
		printUsage();
		exit(1);
	}

	/* Patch the size array depending on command-line options */
	if (!is32kMode)
		maxsize[SECTTYPE_ROM0] = 0x4000;
	if (!isWRA0Mode)
		maxsize[SECTTYPE_WRAM0] = 0x1000;

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
