/* SPDX-License-Identifier: MIT */

#include "asm/main.hpp"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>

#include "error.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp"
#include "parser.hpp"
#include "version.hpp"

#include "asm/charmap.hpp"
#include "asm/fixpoint.hpp"
#include "asm/format.hpp"
#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/opt.hpp"
#include "asm/output.hpp"
#include "asm/rpn.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

#ifdef __clang__
	#if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
		#define __SANITIZE_ADDRESS__
	#endif // __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
#endif     // __clang__

#ifdef __SANITIZE_ADDRESS__
// There are known, non-trivial to fix leaks. We would still like to have `make develop'
// detect memory corruption, though.
extern "C" {
char const *__asan_default_options() {
	return "detect_leaks=0";
}
}
#endif

// Old Bison versions (confirmed for 2.3) do not forward-declare `yyparse` in the generated header
// Unfortunately, macOS still ships 2.3, which is from 2008...
int yyparse();

FILE *dependfile = nullptr;
bool generatedMissingIncludes = false;
bool failedOnMissingInclude = false;
bool generatePhonyDeps = false;
std::string targetFileName;

bool haltNop;
bool warnOnHaltNop;
bool optimizeLoads;
bool warnOnLdOpt;
bool verbose;
bool warnings; // True to enable warnings, false to disable them.

// Escapes Make-special chars from a string
static std::string make_escape(std::string &str) {
	std::string escaped;
	size_t pos = 0;

	for (;;) {
		// All dollars needs to be doubled
		size_t nextPos = str.find("$", pos);
		if (nextPos == std::string::npos)
			break;
		escaped.append(str, pos, nextPos - pos);
		escaped.append("$$");
		pos = nextPos + sizeof("$") - 1;
	}
	escaped.append(str, pos, str.length() - pos);
	return escaped;
}

// Short options
static const char *optstring = "b:D:Eg:Hhi:I:LlM:o:P:p:Q:r:VvW:wX:";

// Variables for the long-only options
static int depType; // Variants of `-M`

// Equivalent long options
// Please keep in the same order as short opts
//
// Also, make sure long opts don't create ambiguity:
// A long opt's name should start with the same letter as its short opt,
// except if it doesn't create any ambiguity (`verbose` versus `version`).
// This is because long opt matching, even to a single char, is prioritized
// over short opt matching
static option const longopts[] = {
    {"binary-digits",    required_argument, nullptr,  'b'},
    {"define",           required_argument, nullptr,  'D'},
    {"export-all",       no_argument,       nullptr,  'E'},
    {"gfx-chars",        required_argument, nullptr,  'g'},
    {"nop-after-halt",   no_argument,       nullptr,  'H'},
    {"halt-without-nop", no_argument,       nullptr,  'h'},
    {"include",          required_argument, nullptr,  'I'},
    {"preserve-ld",      no_argument,       nullptr,  'L'},
    {"auto-ldh",         no_argument,       nullptr,  'l'},
    {"dependfile",       required_argument, nullptr,  'M'},
    {"MG",               no_argument,       &depType, 'G'},
    {"MP",               no_argument,       &depType, 'P'},
    {"MT",               required_argument, &depType, 'T'},
    {"warning",          required_argument, nullptr,  'W'},
    {"MQ",               required_argument, &depType, 'Q'},
    {"output",           required_argument, nullptr,  'o'},
    {"preinclude",       required_argument, nullptr,  'P'},
    {"pad-value",        required_argument, nullptr,  'p'},
    {"q-precision",      required_argument, nullptr,  'Q'},
    {"recursion-depth",  required_argument, nullptr,  'r'},
    {"version",          no_argument,       nullptr,  'V'},
    {"verbose",          no_argument,       nullptr,  'v'},
    {"warning",          required_argument, nullptr,  'W'},
    {"max-errors",       required_argument, nullptr,  'X'},
    {nullptr,            no_argument,       nullptr,  0  }
};

static void printUsage() {
	fputs(
	    "Usage: rgbasm [-EHhLlVvw] [-b chars] [-D name[=value]] [-g chars] [-I path]\n"
	    "              [-M depend_file] [-MG] [-MP] [-MT target_file] [-MQ target_file]\n"
	    "              [-o out_file] [-P include_file] [-p pad_value] [-Q precision]\n"
	    "              [-r depth] [-W warning] [-X max_errors] <file>\n"
	    "Useful options:\n"
	    "    -E, --export-all         export all labels\n"
	    "    -M, --dependfile <path>  set the output dependency file\n"
	    "    -o, --output <path>      set the output object file\n"
	    "    -p, --pad-value <value>  set the value to use for `ds'\n"
	    "    -V, --version            print RGBASM version and exit\n"
	    "    -W, --warning <warning>  enable or disable warnings\n"
	    "\n"
	    "For help, use `man rgbasm' or go to https://rgbds.gbdev.io/docs/\n",
	    stderr
	);
}

int main(int argc, char *argv[]) {
	time_t now = time(nullptr);
	char const *sourceDateEpoch = getenv("SOURCE_DATE_EPOCH");

	// Support SOURCE_DATE_EPOCH for reproducible builds
	// https://reproducible-builds.org/docs/source-date-epoch/
	if (sourceDateEpoch)
		now = (time_t)strtoul(sourceDateEpoch, nullptr, 0);

	// Perform some init for below
	sym_Init(now);

	// Set defaults

	opt_B("01");
	opt_G("0123");
	opt_P(0);
	opt_Q(16);
	haltNop = false;
	warnOnHaltNop = true;
	optimizeLoads = false;
	warnOnLdOpt = true;
	verbose = false;
	warnings = true;
	sym_SetExportAll(false);
	uint32_t maxDepth = DEFAULT_MAX_DEPTH;
	char const *dependFileName = nullptr;
	std::string newTarget;

	// Maximum of 100 errors only applies if rgbasm is printing errors to a terminal.
	if (isatty(STDERR_FILENO))
		maxErrors = 100;

	for (int ch; (ch = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1;) {
		switch (ch) {
			char *endptr;

		case 'b':
			if (strlen(musl_optarg) == 2)
				opt_B(musl_optarg);
			else
				errx("Must specify exactly 2 characters for option 'b'");
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
				opt_G(musl_optarg);
			else
				errx("Must specify exactly 4 characters for option 'g'");
			break;

		case 'H':
			if (warnOnHaltNop)
				warning(
				    WARNING_OBSOLETE, "Automatic `nop` after `halt` (the `-H` flag) is deprecated\n"
				);
			else
				errx("`-H` and `-h` don't make sense together");
			haltNop = true;
			warnOnHaltNop = false;
			break;
		case 'h':
			if (haltNop)
				errx("`-H` and `-h` don't make sense together");
			break;

		// `-i` was the only short option for `--include` until `-I` was
		// introduced to better match the `-I dir` option of gcc and clang.
		case 'i':
			warning(WARNING_OBSOLETE, "`-i` is deprecated; use `-I`\n");
			// fallthrough
		case 'I':
			fstk_AddIncludePath(musl_optarg);
			break;

		case 'L':
			if (optimizeLoads)
				errx("`-L` and `-l` don't make sense together");
			break;
		case 'l':
			if (warnOnLdOpt)
				warning(
				    WARNING_OBSOLETE,
				    "Automatic `ld` to `ldh` optimization (the `-l` flag) is deprecated\n"
				);
			else
				errx("`-L` and `-l` don't make sense together");
			optimizeLoads = true;
			warnOnLdOpt = false;
			break;

		case 'M':
			if (dependfile)
				warnx("Overriding dependfile %s", dependFileName);
			if (!strcmp("-", musl_optarg)) {
				dependfile = stdout;
				dependFileName = "<stdout>";
			} else {
				dependfile = fopen(musl_optarg, "w");
				dependFileName = musl_optarg;
			}
			if (dependfile == nullptr)
				err("Failed to open dependfile \"%s\"", dependFileName);
			break;

		case 'o':
			out_SetFileName(musl_optarg);
			break;

		case 'P':
			fstk_SetPreIncludeFile(musl_optarg);
			break;

			unsigned long padByte;
		case 'p':
			padByte = strtoul(musl_optarg, &endptr, 0);

			if (musl_optarg[0] == '\0' || *endptr != '\0')
				errx("Invalid argument for option 'p'");

			if (padByte > 0xFF)
				errx("Argument for option 'p' must be between 0 and 0xFF");

			opt_P(padByte);
			break;

			unsigned long precision;
			const char *precisionArg;
		case 'Q':
			precisionArg = musl_optarg;
			if (precisionArg[0] == '.')
				precisionArg++;
			precision = strtoul(precisionArg, &endptr, 0);

			if (musl_optarg[0] == '\0' || *endptr != '\0')
				errx("Invalid argument for option 'Q'");

			if (precision < 1 || precision > 31)
				errx("Argument for option 'Q' must be between 1 and 31");

			opt_Q(precision);
			break;

		case 'r':
			maxDepth = strtoul(musl_optarg, &endptr, 0);

			if (musl_optarg[0] == '\0' || *endptr != '\0')
				errx("Invalid argument for option 'r'");
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

			unsigned long maxValue;
		case 'X':
			maxValue = strtoul(musl_optarg, &endptr, 0);

			if (musl_optarg[0] == '\0' || *endptr != '\0')
				errx("Invalid argument for option 'X'");

			if (maxValue > UINT_MAX)
				errx("Argument for option 'X' must be between 0 and %u", UINT_MAX);

			maxErrors = maxValue;
			break;

		// Long-only options
		case 0:
			switch (depType) {
			case 'G':
				generatedMissingIncludes = true;
				break;

			case 'P':
				generatePhonyDeps = true;
				break;

			case 'Q':
			case 'T':
				newTarget = musl_optarg;
				if (depType == 'Q')
					newTarget = make_escape(newTarget);
				if (!targetFileName.empty())
					targetFileName += ' ';
				targetFileName += newTarget;
				break;
			}
			break;

		// Unrecognized options
		default:
			fprintf(stderr, "FATAL: unknown option '%c'\n", ch);
			printUsage();
			exit(1);
		}
	}

	if (targetFileName.empty() && objectName)
		targetFileName = objectName;

	if (argc == musl_optind) {
		fputs(
		    "FATAL: Please specify an input file (pass `-` to read from standard input)\n", stderr
		);
		printUsage();
		exit(1);
	} else if (argc != musl_optind + 1) {
		fputs("FATAL: More than one input file specified\n", stderr);
		printUsage();
		exit(1);
	}

	char const *mainFileName = argv[musl_optind];

	if (verbose)
		printf("Assembling %s\n", mainFileName);

	if (dependfile) {
		if (targetFileName.empty())
			errx("Dependency files can only be created if a target file is specified with either "
			     "-o, -MQ or -MT");

		fprintf(dependfile, "%s: %s\n", targetFileName.c_str(), mainFileName);
	}

	charmap_New(DEFAULT_CHARMAP_NAME, nullptr);

	// Init lexer and file stack, providing file info
	fstk_Init(mainFileName, maxDepth);

	// Perform parse (yyparse is auto-generated from `parser.y`)
	if (yyparse() != 0 && nbErrors == 0)
		nbErrors = 1;

	if (dependfile)
		fclose(dependfile);

	sect_CheckUnionClosed();

	if (nbErrors != 0)
		errx("Assembly aborted (%u error%s)!", nbErrors, nbErrors == 1 ? "" : "s");

	// If parse aborted due to missing an include, and `-MG` was given, exit normally
	if (failedOnMissingInclude)
		return 0;

	// If no path specified, don't write file
	if (objectName != nullptr)
		out_WriteObject();
	return 0;
}
