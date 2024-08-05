/* SPDX-License-Identifier: MIT */

#include "asm/main.hpp"

#include <algorithm>
#include <limits.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "error.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp"
#include "parser.hpp"
#include "version.hpp"

#include "asm/charmap.hpp"
#include "asm/fstack.hpp"
#include "asm/opt.hpp"
#include "asm/output.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

FILE *dependFile = nullptr;            // -M
bool generatedMissingIncludes = false; // -MG
bool generatePhonyDeps = false;        // -MP
std::string targetFileName;            // -MQ, -MT
bool failedOnMissingInclude = false;
bool verbose = false; // -v
bool warnings = true; // -w

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
		pos = nextPos + QUOTEDSTRLEN("$");
	}
	escaped.append(str, pos, str.length() - pos);
	return escaped;
}

// Short options
static char const *optstring = "b:D:Eg:I:M:o:P:p:Q:r:s:VvW:wX:";

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
    {"binary-digits",   required_argument, nullptr,  'b'},
    {"define",          required_argument, nullptr,  'D'},
    {"export-all",      no_argument,       nullptr,  'E'},
    {"gfx-chars",       required_argument, nullptr,  'g'},
    {"include",         required_argument, nullptr,  'I'},
    {"dependfile",      required_argument, nullptr,  'M'},
    {"MG",              no_argument,       &depType, 'G'},
    {"MP",              no_argument,       &depType, 'P'},
    {"MT",              required_argument, &depType, 'T'},
    {"warning",         required_argument, nullptr,  'W'},
    {"MQ",              required_argument, &depType, 'Q'},
    {"output",          required_argument, nullptr,  'o'},
    {"preinclude",      required_argument, nullptr,  'P'},
    {"pad-value",       required_argument, nullptr,  'p'},
    {"q-precision",     required_argument, nullptr,  'Q'},
    {"recursion-depth", required_argument, nullptr,  'r'},
    {"state",           required_argument, nullptr,  's'},
    {"version",         no_argument,       nullptr,  'V'},
    {"verbose",         no_argument,       nullptr,  'v'},
    {"warning",         required_argument, nullptr,  'W'},
    {"max-errors",      required_argument, nullptr,  'X'},
    {nullptr,           no_argument,       nullptr,  0  }
};

static void printUsage() {
	fputs(
	    "Usage: rgbasm [-EVvw] [-b chars] [-D name[=value]] [-g chars] [-I path]\n"
	    "              [-M depend_file] [-MG] [-MP] [-MT target_file] [-MQ target_file]\n"
	    "              [-o out_file] [-P include_file] [-p pad_value] [-Q precision]\n"
	    "              [-r depth] [-s features:state_file] [-W warning] [-X max_errors]\n"
	    "              <file>\n"
	    "Useful options:\n"
	    "    -E, --export-all               export all labels\n"
	    "    -M, --dependfile <path>        set the output dependency file\n"
	    "    -o, --output <path>            set the output object file\n"
	    "    -p, --pad-value <value>        set the value to use for `ds'\n"
	    "    -s, --state <features>:<path>  set an output state file\n"
	    "    -V, --version                  print RGBASM version and exit\n"
	    "    -W, --warning <warning>        enable or disable warnings\n"
	    "\n"
	    "For help, use `man rgbasm' or go to https://rgbds.gbdev.io/docs/\n",
	    stderr
	);
}

int main(int argc, char *argv[]) {
	time_t now = time(nullptr);
	// Support SOURCE_DATE_EPOCH for reproducible builds
	// https://reproducible-builds.org/docs/source-date-epoch/
	if (char const *sourceDateEpoch = getenv("SOURCE_DATE_EPOCH"); sourceDateEpoch)
		now = (time_t)strtoul(sourceDateEpoch, nullptr, 0);

	Defer closeDependFile{[&] {
		if (dependFile)
			fclose(dependFile);
	}};

	// Perform some init for below
	sym_Init(now);

	// Set defaults
	opt_B("01");
	opt_G("0123");
	opt_P(0);
	opt_Q(16);
	sym_SetExportAll(false);
	uint32_t maxDepth = DEFAULT_MAX_DEPTH;
	char const *dependFileName = nullptr;
	std::unordered_map<std::string, std::vector<StateFeature>> stateFileSpecs;
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
				sym_AddString(musl_optarg, std::make_shared<std::string>(equals + 1));
			} else {
				sym_AddString(musl_optarg, std::make_shared<std::string>("1"));
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

		case 'I':
			fstk_AddIncludePath(musl_optarg);
			break;

		case 'M':
			if (dependFile)
				warnx("Overriding dependfile %s", dependFileName);
			if (strcmp("-", musl_optarg)) {
				dependFile = fopen(musl_optarg, "w");
				dependFileName = musl_optarg;
			} else {
				dependFile = stdout;
				dependFileName = "<stdout>";
			}
			if (dependFile == nullptr)
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
			char const *precisionArg;
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

		case 's': {
			// Split "<features>:<name>" so `musl_optarg` is "<features>" and `name` is "<name>"
			char *name = strchr(musl_optarg, ':');
			if (!name)
				errx("Invalid argument for option 's'");
			*name++ = '\0';

			std::vector<StateFeature> features;
			for (char *feature = musl_optarg; feature;) {
				// Split "<feature>,<rest>" so `feature` is "<feature>" and `next` is "<rest>"
				char *next = strchr(feature, ',');
				if (next)
					*next++ = '\0';
				// Trim whitespace from the beginning of `feature`...
				feature += strspn(feature, " \t");
				// ...and from the end
				if (char *end = strpbrk(feature, " \t"); end)
					*end = '\0';
				// A feature must be specified
				if (*feature == '\0')
					errx("Empty feature for option 's'");
				// Parse the `feature` and update the `features` list
				if (!strcasecmp(feature, "all")) {
					if (!features.empty())
						warnx("Redundant feature before \"%s\" for option 's'", feature);
					features.assign({STATE_EQU, STATE_VAR, STATE_EQUS, STATE_CHAR, STATE_MACRO});
				} else {
					StateFeature value = !strcasecmp(feature, "equ")     ? STATE_EQU
					                     : !strcasecmp(feature, "var")   ? STATE_VAR
					                     : !strcasecmp(feature, "equs")  ? STATE_EQUS
					                     : !strcasecmp(feature, "char")  ? STATE_CHAR
					                     : !strcasecmp(feature, "macro") ? STATE_MACRO
					                                                    : NB_STATE_FEATURES;
					if (value == NB_STATE_FEATURES) {
						errx("Invalid feature for option 's': \"%s\"", feature);
					} else if (std::find(RANGE(features), value) != features.end()) {
						warnx("Ignoring duplicate feature for option 's': \"%s\"", feature);
					} else {
						features.push_back(value);
					}
				}
				feature = next;
			}

			if (stateFileSpecs.find(name) != stateFileSpecs.end())
				warnx("Overriding state filename %s", name);
			if (verbose)
				printf("State filename %s\n", name);
			stateFileSpecs.emplace(name, std::move(features));
			break;
		}

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
			printUsage();
			exit(1);
		}
	}

	if (targetFileName.empty() && !objectFileName.empty())
		targetFileName = objectFileName;

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

	std::string mainFileName = argv[musl_optind];

	if (verbose)
		printf("Assembling %s\n", mainFileName.c_str());

	if (dependFile) {
		if (targetFileName.empty())
			errx("Dependency files can only be created if a target file is specified with either "
			     "-o, -MQ or -MT");

		fprintf(dependFile, "%s: %s\n", targetFileName.c_str(), mainFileName.c_str());
	}

	charmap_New(DEFAULT_CHARMAP_NAME, nullptr);

	// Init lexer and file stack, providing file info
	fstk_Init(mainFileName, maxDepth);

	// Perform parse (`yy::parser` is auto-generated from `parser.y`)
	if (yy::parser parser; parser.parse() != 0 && nbErrors == 0)
		nbErrors = 1;

	sect_CheckUnionClosed();

	if (nbErrors != 0)
		errx("Assembly aborted (%u error%s)!", nbErrors, nbErrors == 1 ? "" : "s");

	// If parse aborted due to missing an include, and `-MG` was given, exit normally
	if (failedOnMissingInclude)
		return 0;

	out_WriteObject();

	for (auto [name, features] : stateFileSpecs)
		out_WriteState(name, features);

	return 0;
}
