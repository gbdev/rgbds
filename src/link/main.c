/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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
char       *linkerScriptName; /* -l */
char const *mapFileName;      /* -m */
char const *symFileName;      /* -n */
char const *overlayFileName;  /* -O */
char const *outputFileName;   /* -o */
uint8_t padValue;             /* -p */
uint16_t scrambleROMBanks;    /* -S */
bool is32kMode;               /* -t */
bool beVerbose;               /* -v */
bool scrambleWRAMBanks;       /* -W */
bool isWRA0Mode;              /* -w */
bool disablePadding;          /* -x */

static uint32_t nbErrors = 0;

/***** Helper function to dump a file stack to stderr *****/

char const *dumpFileStack(struct FileStackNode const *node)
{
	char const *lastName;

	if (node->parent) {
		lastName = dumpFileStack(node->parent);
		/* REPT nodes use their parent's name */
		if (node->type != NODE_REPT)
			lastName = node->name;
		fprintf(stderr, "(%" PRIu32 ") -> %s", node->lineNo, lastName);
		if (node->type == NODE_REPT) {
			for (uint32_t i = 0; i < node->reptDepth; i++)
				fprintf(stderr, "::REPT~%" PRIu32, node->iters[i]);
		}
	} else {
		assert(node->type != NODE_REPT);
		lastName = node->name;
		fputs(lastName, stderr);
	}

	return lastName;
}

void warning(struct FileStackNode const *where, uint32_t lineNo, char const *fmt, ...)
{
	va_list ap;

	fputs("warning: ", stderr);
	if (where) {
		dumpFileStack(where);
		fprintf(stderr, "(%" PRIu32 "): ", lineNo);
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
}

void error(struct FileStackNode const *where, uint32_t lineNo, char const *fmt, ...)
{
	va_list ap;

	fputs("error: ", stderr);
	if (where) {
		dumpFileStack(where);
		fprintf(stderr, "(%" PRIu32 "): ", lineNo);
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != UINT32_MAX)
		nbErrors++;
}

_Noreturn void fatal(struct FileStackNode const *where, uint32_t lineNo, char const *fmt, ...)
{
	va_list ap;

	fputs("fatal: ", stderr);
	if (where) {
		dumpFileStack(where);
		fprintf(stderr, "(%" PRIu32 "): ", lineNo);
	}
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != UINT32_MAX)
		nbErrors++;

	fprintf(stderr, "Linking aborted after %" PRIu32 " error%s\n", nbErrors,
		nbErrors != 1 ? "s" : "");
	exit(1);
}

FILE *openFile(char const *fileName, char const *mode)
{
	if (!fileName)
		return NULL;

	FILE *file;
	if (strcmp(fileName, "-") != 0)
		file = fopen(fileName, mode);
	else if (mode[0] == 'r')
		file = fdopen(0, mode);
	else
		file = fdopen(1, mode);

	if (!file)
		err(1, "Could not open file \"%s\"", fileName);

	return file;
}

/* Short options */
static const char *optstring = "dl:m:n:O:o:p:S:s:tVvWwx";

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
	{ "dmg",           no_argument,       NULL, 'd' },
	{ "linkerscript",  required_argument, NULL, 'l' },
	{ "map",           required_argument, NULL, 'm' },
	{ "sym",           required_argument, NULL, 'n' },
	{ "overlay",       required_argument, NULL, 'O' },
	{ "output",        required_argument, NULL, 'o' },
	{ "pad",           required_argument, NULL, 'p' },
	{ "scramble-rom",  required_argument, NULL, 'S' },
	{ "smart",         required_argument, NULL, 's' },
	{ "tiny",          no_argument,       NULL, 't' },
	{ "version",       no_argument,       NULL, 'V' },
	{ "verbose",       no_argument,       NULL, 'v' },
	{ "scramble-wram", no_argument,       NULL, 'W' },
	{ "wramx",         no_argument,       NULL, 'w' },
	{ "nopad",         no_argument,       NULL, 'x' },
	{ NULL,            no_argument,       NULL, 0   }
};

/**
 * Prints the program's usage to stdout.
 */
static void printUsage(void)
{
	fputs(
"Usage: rgblink [-dtVvwx] [-l script] [-m map_file] [-n sym_file]\n"
"               [-O overlay_file] [-o out_file] [-p pad_value] [-s symbol]\n"
"               <file> ...\n"
"Useful options:\n"
"    -l, --linkerscript <path>  set the input linker script\n"
"    -m, --map <path>           set the output map file\n"
"    -n, --sym <path>           set the output symbol list file\n"
"    -o, --output <path>        set the output file\n"
"    -p, --pad <value>          set the value to pad between sections with\n"
"    -x, --nopad                disable padding of output binary\n"
"    -V, --version              print RGBLINK version and exits\n"
"\n"
"For help, use `man rgblink' or go to https://rgbds.gbdev.io/docs/\n",
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
	char *endptr; /* For error checking with `strtoul` */
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
			linkerScriptName = musl_optarg;
			break;
		case 'm':
			mapFileName = musl_optarg;
			break;
		case 'n':
			symFileName = musl_optarg;
			break;
		case 'O':
			overlayFileName = musl_optarg;
			break;
		case 'o':
			outputFileName = musl_optarg;
			break;
		case 'p':
			value = strtoul(musl_optarg, &endptr, 0);
			if (musl_optarg[0] == '\0' || *endptr != '\0') {
				error(NULL, 0, "Invalid argument for option 'p'");
				value = 0xFF;
			}
			if (value > 0xFF) {
				error(NULL, 0, "Argument for 'p' must be a byte (between 0 and 0xFF)");
				value = 0xFF;
			}
			padValue = value;
			break;
		case 'S':
			value = strtoul(musl_optarg, &endptr, 0);
			if (musl_optarg[0] == '\0' || *endptr != '\0') {
				error(NULL, 0, "Invalid argument for option 'S'");
				value = 256;
			}
			if (value < 2) {
				error(NULL, 0, "Argument for 'S' must be at least 2");
				value = 256;
			}
			if (value > 0xFFFF) {
				error(NULL, 0, "Argument for 'S' must be a word (between 2 and 0xFFFF)");
				value = 256;
			}
			scrambleROMBanks = value;
			break;
		case 's':
			/* FIXME: nobody knows what this does, figure it out */
			(void)musl_optarg;
			warning(NULL, 0, "Nobody has any idea what `-s` does");
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
		case 'W':
			scrambleWRAMBanks = true;
			break;
		case 'w':
			isWRA0Mode = true;
			break;
		case 'x':
			disablePadding = true;
			/* implies tiny mode */
			is32kMode = true;
			break;
		default:
			printUsage();
			exit(1);
		}
	}

	int curArgIndex = musl_optind;

	/* If no input files were specified, the user must have screwed up */
	if (curArgIndex == argc) {
		fputs("fatal: no input files\n", stderr);
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
	for (obj_Setup(argc - curArgIndex); curArgIndex < argc; curArgIndex++)
		obj_ReadFile(argv[curArgIndex], argc - curArgIndex - 1);

	/* then process them, */
	obj_DoSanityChecks();
	assign_AssignSections();
	obj_CheckAssertions();
	assign_Cleanup();

	/* and finally output the result. */
	patch_ApplyPatches();
	if (nbErrors) {
		fprintf(stderr, "Linking failed with %" PRIu32 " error%s\n",
			nbErrors, nbErrors != 1 ? "s" : "");
		exit(1);
	}
	out_WriteFiles();

	/* Do cleanup before quitting, though. */
	cleanup();
}
