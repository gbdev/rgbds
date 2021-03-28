/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "asm/charmap.h"
#include "asm/format.h"
#include "asm/fstack.h"
#include "asm/lexer.h"
#include "asm/main.h"
#include "asm/opt.h"
#include "asm/output.h"
#include "asm/rpn.h"
#include "asm/symbol.h"
#include "asm/warning.h"
#include "parser.h"

#include "extern/err.h"
#include "extern/getopt.h"

#include "helpers.h"
#include "version.h"

// Old Bison versions (confirmed for 2.3) do not forward-declare `yyparse` in the generated header
// Unfortunately, macOS still ships 2.3, which is from 2008...
int yyparse(void);

#if defined(YYDEBUG) && YYDEBUG
extern int yydebug;
#endif

FILE * dependfile;
bool oGeneratedMissingIncludes;
bool oFailedOnMissingInclude;
bool oGeneratePhonyDeps;
char *tzTargetFileName;

bool haltnop;
bool optimizeloads;
bool verbose;
bool warnings; /* True to enable warnings, false to disable them. */

/* Escapes Make-special chars from a string */
static char *make_escape(const char *str)
{
	char * const escaped_str = malloc(strlen(str) * 2 + 1);
	char *dest = escaped_str;

	if (escaped_str == NULL)
		err(1, "%s: Failed to allocate memory", __func__);

	while (*str) {
		/* All dollars needs to be doubled */
		if (*str == '$')
			*dest++ = '$';
		*dest++ = *str++;
	}
	*dest = '\0';

	return escaped_str;
}

/* Short options */
static const char *optstring = "b:D:Eg:hi:LM:o:p:r:VvW:w";

/* Variables for the long-only options */
static int depType; /* Variants of `-M` */

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
	{ "binary-digits",    required_argument, NULL,     'b' },
	{ "define",           required_argument, NULL,     'D' },
	{ "export-all",       no_argument,       NULL,     'E' },
	{ "gfx-chars",        required_argument, NULL,     'g' },
	{ "halt-without-nop", no_argument,       NULL,     'h' },
	{ "include",          required_argument, NULL,     'i' },
	{ "preserve-ld",      no_argument,       NULL,     'L' },
	{ "dependfile",       required_argument, NULL,     'M' },
	{ "MG",               no_argument,       &depType, 'G' },
	{ "MP",               no_argument,       &depType, 'P' },
	{ "MT",               required_argument, &depType, 'T' },
	{ "MQ",               required_argument, &depType, 'Q' },
	{ "output",           required_argument, NULL,     'o' },
	{ "pad-value",        required_argument, NULL,     'p' },
	{ "recursion-depth",  required_argument, NULL,     'r' },
	{ "version",          no_argument,       NULL,     'V' },
	{ "verbose",          no_argument,       NULL,     'v' },
	{ "warning",          required_argument, NULL,     'W' },
	{ NULL,               no_argument,       NULL,     0   }
};

static void print_usage(void)
{
	fputs(
"Usage: rgbasm [-EhLVvw] [-b chars] [-D name[=value]] [-g chars] [-i path]\n"
"              [-M depend_file] [-MG] [-MP] [-MT target_file] [-MQ target_file]\n"
"              [-o out_file] [-p pad_value] [-r depth] [-W warning] <file>\n"
"Useful options:\n"
"    -E, --export-all         export all labels\n"
"    -M, --dependfile <path>  set the output dependency file\n"
"    -o, --output <path>      set the output object file\n"
"    -p, --pad-value <value>  set the value to use for `ds'\n"
"    -V, --version            print RGBASM version and exit\n"
"    -W, --warning <warning>  enable or disable warnings\n"
"\n"
"For help, use `man rgbasm' or go to https://rgbds.gbdev.io/docs/\n",
	      stderr);
	exit(1);
}

int main(int argc, char *argv[])
{
	int ch;
	char *ep;

	time_t now = time(NULL);
	char const *sourceDateEpoch = getenv("SOURCE_DATE_EPOCH");

	/*
	 * Support SOURCE_DATE_EPOCH for reproducible builds
	 * https://reproducible-builds.org/docs/source-date-epoch/
	 */
	if (sourceDateEpoch)
		now = (time_t)strtoul(sourceDateEpoch, NULL, 0);

	dependfile = NULL;

#if defined(YYDEBUG) && YYDEBUG
	yydebug = 1;
#endif

	// Perform some init for below
	sym_Init(now);

	// Set defaults

	oGeneratePhonyDeps = false;
	oGeneratedMissingIncludes = false;
	oFailedOnMissingInclude = false;
	tzTargetFileName = NULL;

	opt_B("01");
	opt_G("0123");
	opt_P(0);
	optimizeloads = true;
	haltnop = true;
	verbose = false;
	warnings = true;
	sym_SetExportAll(false);
	uint32_t maxRecursionDepth = 64;
	size_t nTargetFileNameLen = 0;

	while ((ch = musl_getopt_long_only(argc, argv, optstring, longopts, NULL)) != -1) {
		switch (ch) {
		case 'b':
			if (strlen(musl_optarg) == 2)
				opt_B(&musl_optarg[1]);
			else
				errx(1, "Must specify exactly 2 characters for option 'b'");
			break;

			char *equals;
		case 'D':
			equals = strchr(musl_optarg, '=');
			if (equals) {
				*equals = '\0';
				sym_AddString(musl_optarg, equals + 1);
			} else {
				sym_AddString(musl_optarg, "1");
			}
			break;

		case 'E':
			sym_SetExportAll(true);
			break;

		case 'g':
			if (strlen(musl_optarg) == 4)
				opt_G(&musl_optarg[1]);
			else
				errx(1, "Must specify exactly 4 characters for option 'g'");
			break;

		case 'h':
			haltnop = false;
			break;

		case 'i':
			fstk_AddIncludePath(musl_optarg);
			break;

		case 'L':
			optimizeloads = false;
			break;

		case 'M':
			if (!strcmp("-", musl_optarg))
				dependfile = stdout;
			else
				dependfile = fopen(musl_optarg, "w");
			if (dependfile == NULL)
				err(1, "Could not open dependfile %s", musl_optarg);
			break;

		case 'o':
			out_SetFileName(musl_optarg);
			break;

			unsigned long fill;
		case 'p':
			fill = strtoul(musl_optarg, &ep, 0);

			if (musl_optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'p'");

			if (fill < 0 || fill > 0xFF)
				errx(1, "Argument for option 'p' must be between 0 and 0xFF");

			opt_P(fill);
			break;

		case 'r':
			maxRecursionDepth = strtoul(musl_optarg, &ep, 0);

			if (musl_optarg[0] == '\0' || *ep != '\0')
				errx(1, "Invalid argument for option 'r'");
			break;

		case 'V':
			printf("rgbasm %s\n", get_package_version_string());
			exit(0);
		case 'v':
			verbose = true;
			break;

		case 'W':
			processWarningFlag(musl_optarg);
			break;

		case 'w':
			warnings = false;
			break;

		/* Long-only options */
		case 0:
			switch (depType) {
			case 'G':
				oGeneratedMissingIncludes = true;
				break;

			case 'P':
				oGeneratePhonyDeps = true;
				break;

			case 'Q':
			case 'T':
				if (musl_optind == argc)
					errx(1, "-M%c takes a target file name argument", depType);
				ep = musl_optarg;
				if (depType == 'Q')
					ep = make_escape(ep);

				nTargetFileNameLen += strlen(ep) + 1;
				if (!tzTargetFileName) {
					/* On first alloc, make an empty str */
					tzTargetFileName = malloc(nTargetFileNameLen + 1);
					if (tzTargetFileName)
						*tzTargetFileName = '\0';
				} else {
					tzTargetFileName = realloc(tzTargetFileName,
								   nTargetFileNameLen + 1);
				}
				if (tzTargetFileName == NULL)
					err(1, "Cannot append new file to target file list");
				strcat(tzTargetFileName, ep);
				if (depType == 'Q')
					free(ep);
				char *ptr = tzTargetFileName + strlen(tzTargetFileName);

				*ptr++ = ' ';
				*ptr = '\0';
				break;
			}
			break;

		/* Unrecognized options */
		default:
			print_usage();
			/* NOTREACHED */
		}
	}

	if (tzTargetFileName == NULL)
		tzTargetFileName = tzObjectname;

	if (argc == musl_optind) {
		fputs("FATAL: No input files\n", stderr);
		print_usage();
	} else if (argc != musl_optind + 1) {
		fputs("FATAL: More than one input file given\n", stderr);
		print_usage();
	}

	char const *mainFileName = argv[musl_optind];

	if (verbose)
		printf("Assembling %s\n", mainFileName);

	if (dependfile) {
		if (!tzTargetFileName)
			errx(1, "Dependency files can only be created if a target file is specified with either -o, -MQ or -MT\n");

		fprintf(dependfile, "%s: %s\n", tzTargetFileName, mainFileName);
	}

	charmap_New("main", NULL);

	// Init lexer and file stack, prodiving file info
	lexer_Init();
	fstk_Init(mainFileName, maxRecursionDepth);

	// Perform parse (yyparse is auto-generated from `parser.y`)
	if (yyparse() != 0 && nbErrors == 0)
		nbErrors = 1;

	if (dependfile)
		fclose(dependfile);

	sect_CheckUnionClosed();

	if (nbErrors != 0)
		errx(1, "Assembly aborted (%u error%s)!", nbErrors,
			nbErrors == 1 ? "" : "s");

	// If parse aborted due to missing an include, and `-MG` was given, exit normally
	if (oFailedOnMissingInclude)
		return 0;

	/* If no path specified, don't write file */
	if (tzObjectname != NULL)
		out_WriteObject();
	return 0;
}
