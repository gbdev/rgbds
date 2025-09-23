// SPDX-License-Identifier: MIT

#include "asm/main.hpp"

#include <algorithm>
#include <errno.h>
#include <inttypes.h>
#include <memory>
#include <optional>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "backtrace.hpp"
#include "diagnostics.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp"
#include "parser.hpp" // Generated from parser.y
#include "platform.hpp"
#include "style.hpp"
#include "usage.hpp"
#include "util.hpp" // UpperMap
#include "verbosity.hpp"
#include "version.hpp"

#include "asm/charmap.hpp"
#include "asm/fstack.hpp"
#include "asm/opt.hpp"
#include "asm/output.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

Options options;

static char const *dependFileName = nullptr;                                      // -M
static std::unordered_map<std::string, std::vector<StateFeature>> stateFileSpecs; // -s

// Short options
static char const *optstring = "B:b:D:Eg:hI:M:o:P:p:Q:r:s:VvW:wX:";

// Variables for the long-only options
static int longOpt; // `--color` and variants of `-M`

// Equivalent long options
// Please keep in the same order as short opts.
// Also, make sure long opts don't create ambiguity:
// A long opt's name should start with the same letter as its short opt,
// except if it doesn't create any ambiguity (`verbose` versus `version`).
// This is because long opt matching, even to a single char, is prioritized
// over short opt matching.
static option const longopts[] = {
    {"backtrace",       required_argument, nullptr,  'B'},
    {"binary-digits",   required_argument, nullptr,  'b'},
    {"define",          required_argument, nullptr,  'D'},
    {"export-all",      no_argument,       nullptr,  'E'},
    {"gfx-chars",       required_argument, nullptr,  'g'},
    {"help",            no_argument,       nullptr,  'h'},
    {"include",         required_argument, nullptr,  'I'},
    {"dependfile",      required_argument, nullptr,  'M'},
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
    {"color",           required_argument, &longOpt, 'c'},
    {"MC",              no_argument,       &longOpt, 'C'},
    {"MG",              no_argument,       &longOpt, 'G'},
    {"MP",              no_argument,       &longOpt, 'P'},
    {"MQ",              required_argument, &longOpt, 'Q'},
    {"MT",              required_argument, &longOpt, 'T'},
    {nullptr,           no_argument,       nullptr,  0  },
};

// clang-format off: nested initializers
static Usage usage = {
    .name = "rgbasm",
    .flags = {
        "[-EhVvw]", "[-B depth]", "[-b chars]", "[-D name[=value]]", "[-g chars]", "[-I path]",
        "[-M depend_file]", "[-MC]", "[-MG]", "[-MP]", "[-MT target_file]", "[-MQ target_file]",
        "[-o out_file]", "[-P include_file]", "[-p pad_value]", "[-Q precision]", "[-r depth]",
        "[-s features:state_file]", "[-W warning]", "[-X max_errors]", "<file>",
    },
    .options = {
        {{"-E", "--export-all"}, {"export all labels"}},
        {{"-M", "--dependfile <path>"}, {"set the output dependency file"}},
        {{"-o", "--output <path>"}, {"set the output object file"}},
        {{"-p", "--pad-value <value>"}, {"set the value to use for `DS`"}},
        {{"-s", "--state <features>:<path>"}, {"set an output state file"}},
        {{"-V", "--version"}, {"print RGBASM version and exit"}},
        {{"-W", "--warning <warning>"}, {"enable or disable warnings"}},
    },
};
// clang-format on

// LCOV_EXCL_START
static void verboseOutputConfig(int argc, char *argv[]) {
	if (!checkVerbosity(VERB_CONFIG)) {
		return;
	}

	style_Set(stderr, STYLE_MAGENTA, false);

	fprintf(stderr, "rgbasm %s\n", get_package_version_string());

	printVVVVVVerbosity();

	fputs("Options:\n", stderr);
	// -E/--export-all
	if (options.exportAll) {
		fputs("\tExport all labels by default\n", stderr);
	}
	// -b/--binary-digits
	if (options.binDigits[0] != '0' || options.binDigits[1] != '1') {
		fprintf(
		    stderr, "\tBinary digits: '%c', '%c'\n", options.binDigits[0], options.binDigits[1]
		);
	}
	// -g/--gfx-chars
	if (options.gfxDigits[0] != '0' || options.gfxDigits[1] != '1' || options.gfxDigits[2] != '2'
	    || options.gfxDigits[3] != '3') {
		fprintf(
		    stderr,
		    "\tGraphics characters: '%c', '%c', '%c', '%c'\n",
		    options.gfxDigits[0],
		    options.gfxDigits[1],
		    options.gfxDigits[2],
		    options.gfxDigits[3]
		);
	}
	// -Q/--q-precision
	fprintf(
	    stderr,
	    "\tFixed-point precision: Q%d.%" PRIu8 "\n",
	    32 - options.fixPrecision,
	    options.fixPrecision
	);
	// -p/--pad-value
	fprintf(stderr, "\tPad value: 0x%02" PRIx8 "\n", options.padByte);
	// -r/--recursion-depth
	fprintf(stderr, "\tMaximum recursion depth %zu\n", options.maxRecursionDepth);
	// -X/--max-errors
	if (options.maxErrors) {
		fprintf(stderr, "\tMaximum %" PRIu64 " errors\n", options.maxErrors);
	}
	// -D/--define
	static bool hasDefines = false; // `static` so `sym_ForEach` callback can see it
	sym_ForEach([](Symbol &sym) {
		if (!sym.isBuiltin && sym.type == SYM_EQUS) {
			if (!hasDefines) {
				fputs("\tDefinitions:\n", stderr);
				hasDefines = true;
			}
			fprintf(stderr, "\t - def %s equs \"%s\"\n", sym.name.c_str(), sym.getEqus()->c_str());
		}
	});
	// -s/--state
	if (!stateFileSpecs.empty()) {
		fputs("\tOutput state files:\n", stderr);
		static char const *featureNames[NB_STATE_FEATURES] = {
		    "equ",
		    "var",
		    "equs",
		    "char",
		    "macro",
		};
		for (auto const &[name, features] : stateFileSpecs) {
			fprintf(stderr, "\t - %s: ", name == "-" ? "<stdout>" : name.c_str());
			for (size_t i = 0; i < features.size(); ++i) {
				if (i > 0) {
					fputs(", ", stderr);
				}
				fputs(featureNames[features[i]], stderr);
			}
			putc('\n', stderr);
		}
	}
	// asmfile
	if (musl_optind < argc) {
		fprintf(stderr, "\tInput asm file: %s", argv[musl_optind]);
		if (musl_optind + 1 < argc) {
			fprintf(stderr, " (and %d more)", argc - musl_optind - 1);
		}
		putc('\n', stderr);
	}
	// -o/--output
	if (!options.objectFileName.empty()) {
		fprintf(stderr, "\tOutput object file: %s\n", options.objectFileName.c_str());
	}
	fstk_VerboseOutputConfig();
	if (dependFileName) {
		fprintf(
		    stderr,
		    "\tOutput dependency file: %s\n",
		    strcmp(dependFileName, "-") ? dependFileName : "<stdout>"
		);
		// -MT or -MQ
		if (!options.targetFileName.empty()) {
			fprintf(stderr, "\tTarget file(s): %s\n", options.targetFileName.c_str());
		}
		// -MG or -MC
		switch (options.missingIncludeState) {
		case INC_ERROR:
			fputs("\tExit with an error on a missing dependency\n", stderr);
			break;
		case GEN_EXIT:
			fputs("\tExit normally on a missing dependency\n", stderr);
			break;
		case GEN_CONTINUE:
			fputs("\tContinue processing after a missing dependency\n", stderr);
			break;
		}
		// -MP
		if (options.generatePhonyDeps) {
			fputs("\tGenerate phony dependencies\n", stderr);
		}
	}
	fputs("Ready.\n", stderr);

	style_Reset(stderr);
}
// LCOV_EXCL_STOP

static std::string escapeMakeChars(std::string &str) {
	std::string escaped;
	size_t pos = 0;
	for (;;) {
		// All dollars needs to be doubled
		size_t nextPos = str.find('$', pos);
		if (nextPos == std::string::npos) {
			break;
		}
		escaped.append(str, pos, nextPos - pos);
		escaped.append("$$");
		pos = nextPos + literal_strlen("$");
	}
	escaped.append(str, pos, str.length() - pos);
	return escaped;
}

// Parse a comma-separated string of '-s/--state' features
static std::vector<StateFeature> parseStateFeatures(char *str) {
	std::vector<StateFeature> features;
	for (char *feature = str; feature;) {
		// Split "<feature>,<rest>" so `feature` is "<feature>" and `next` is "<rest>"
		char *next = strchr(feature, ',');
		if (next) {
			*next++ = '\0';
		}
		// Trim blank spaces from the beginning of `feature`...
		feature += strspn(feature, " \t");
		// ...and from the end
		if (char *end = strpbrk(feature, " \t"); end) {
			*end = '\0';
		}
		// A feature must be specified
		if (*feature == '\0') {
			fatal("Empty feature for option '-s'");
		}
		// Parse the `feature` and update the `features` list
		static UpperMap<StateFeature> const featureNames{
		    {"EQU",   STATE_EQU  },
		    {"VAR",   STATE_VAR  },
		    {"EQUS",  STATE_EQUS },
		    {"CHAR",  STATE_CHAR },
		    {"MACRO", STATE_MACRO},
		};
		if (!strcasecmp(feature, "all")) {
			if (!features.empty()) {
				warnx("Redundant feature before \"%s\" for option '-s'", feature);
			}
			features.assign({STATE_EQU, STATE_VAR, STATE_EQUS, STATE_CHAR, STATE_MACRO});
		} else if (auto search = featureNames.find(feature); search == featureNames.end()) {
			fatal("Invalid feature for option '-s': \"%s\"", feature);
		} else if (StateFeature value = search->second;
		           std::find(RANGE(features), value) != features.end()) {
			warnx("Ignoring duplicate feature for option '-s': \"%s\"", feature);
		} else {
			features.push_back(value);
		}
		feature = next;
	}
	return features;
}

int main(int argc, char *argv[]) {
	// Support SOURCE_DATE_EPOCH for reproducible builds
	// https://reproducible-builds.org/docs/source-date-epoch/
	time_t now = time(nullptr);
	if (char const *sourceDateEpoch = getenv("SOURCE_DATE_EPOCH"); sourceDateEpoch) {
		// Use `strtoul`, not `parseWholeNumber`, because SOURCE_DATE_EPOCH does
		// not conventionally support our custom base prefixes
		now = static_cast<time_t>(strtoul(sourceDateEpoch, nullptr, 0));
	}
	sym_Init(now);

	// Maximum of 100 errors only applies if rgbasm is printing errors to a terminal
	if (isatty(STDERR_FILENO)) {
		options.maxErrors = 100; // LCOV_EXCL_LINE
	}

	// Parse CLI options
	for (int ch; (ch = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1;) {
		switch (ch) {
		case 'B':
			if (!trace_ParseTraceDepth(musl_optarg)) {
				fatal("Invalid argument for option '-B'");
			}
			break;

		case 'b':
			if (strlen(musl_optarg) == 2) {
				opt_B(musl_optarg);
			} else {
				fatal("Must specify exactly 2 characters for option '-b'");
			}
			break;

		case 'D': {
			char *equals = strchr(musl_optarg, '=');
			if (equals) {
				*equals = '\0';
				sym_AddString(musl_optarg, std::make_shared<std::string>(equals + 1));
			} else {
				sym_AddString(musl_optarg, std::make_shared<std::string>("1"));
			}
			break;
		}

		case 'E':
			options.exportAll = true;
			break;

		case 'g':
			if (strlen(musl_optarg) == 4) {
				opt_G(musl_optarg);
			} else {
				fatal("Must specify exactly 4 characters for option '-g'");
			}
			break;

			// LCOV_EXCL_START
		case 'h':
			usage.printAndExit(0);
			// LCOV_EXCL_STOP

		case 'I':
			fstk_AddIncludePath(musl_optarg);
			break;

		case 'M':
			if (dependFileName) {
				warnx(
				    "Overriding dependency file \"%s\"",
				    strcmp(dependFileName, "-") ? dependFileName : "<stdout>"
				);
			}
			dependFileName = musl_optarg;
			break;

		case 'o':
			if (!options.objectFileName.empty()) {
				warnx("Overriding output file \"%s\"", options.objectFileName.c_str());
			}
			options.objectFileName = musl_optarg;
			break;

		case 'P':
			fstk_AddPreIncludeFile(musl_optarg);
			break;

		case 'p':
			if (std::optional<uint64_t> padByte = parseWholeNumber(musl_optarg); !padByte) {
				fatal("Invalid argument for option '-p'");
			} else if (*padByte > 0xFF) {
				fatal("Argument for option '-p' must be between 0 and 0xFF");
			} else {
				opt_P(*padByte);
			}
			break;

		case 'Q': {
			char const *precisionArg = musl_optarg;
			if (precisionArg[0] == '.') {
				++precisionArg;
			}

			if (std::optional<uint64_t> precision = parseWholeNumber(precisionArg); !precision) {
				fatal("Invalid argument for option '-Q'");
			} else if (*precision < 1 || *precision > 31) {
				fatal("Argument for option '-Q' must be between 1 and 31");
			} else {
				opt_Q(*precision);
			}
			break;
		}

		case 'r':
			if (std::optional<uint64_t> maxDepth = parseWholeNumber(musl_optarg); !maxDepth) {
				fatal("Invalid argument for option '-r'");
			} else if (errno == ERANGE) {
				fatal("Argument for option '-r' is out of range");
			} else {
				options.maxRecursionDepth = *maxDepth;
			}
			break;

		case 's': {
			// Split "<features>:<name>" so `musl_optarg` is "<features>" and `name` is "<name>"
			char *name = strchr(musl_optarg, ':');
			if (!name) {
				fatal("Invalid argument for option '-s'");
			}
			*name++ = '\0';

			std::vector<StateFeature> features = parseStateFeatures(musl_optarg);

			if (stateFileSpecs.find(name) != stateFileSpecs.end()) {
				warnx("Overriding state file \"%s\"", name);
			}
			stateFileSpecs.emplace(name, std::move(features));
			break;
		}

			// LCOV_EXCL_START
		case 'V':
			printf("rgbasm %s\n", get_package_version_string());
			exit(0);

		case 'v':
			incrementVerbosity();
			break;
			// LCOV_EXCL_STOP

		case 'W':
			opt_W(musl_optarg);
			break;

		case 'w':
			warnings.state.warningsEnabled = false;
			break;

		case 'X':
			if (std::optional<uint64_t> maxErrors = parseWholeNumber(musl_optarg); !maxErrors) {
				fatal("Invalid argument for option '-X'");
			} else if (*maxErrors > UINT64_MAX) {
				fatal("Argument for option '-X' must be between 0 and %" PRIu64, UINT64_MAX);
			} else {
				options.maxErrors = *maxErrors;
			}
			break;

		case 0: // Long-only options
			switch (longOpt) {
			case 'c':
				if (!style_Parse(musl_optarg)) {
					fatal("Invalid argument for option '--color'");
				}
				break;

			case 'C':
				options.missingIncludeState = GEN_CONTINUE;
				break;

			case 'G':
				options.missingIncludeState = GEN_EXIT;
				break;

			case 'P':
				options.generatePhonyDeps = true;
				break;

			case 'Q':
			case 'T': {
				std::string newTarget = musl_optarg;
				if (longOpt == 'Q') {
					newTarget = escapeMakeChars(newTarget);
				}
				if (!options.targetFileName.empty()) {
					options.targetFileName += ' ';
				}
				options.targetFileName += newTarget;
				break;
			}
			}
			break;

			// LCOV_EXCL_START
		default:
			usage.printAndExit(1);
			// LCOV_EXCL_STOP
		}
	}

	if (options.targetFileName.empty() && !options.objectFileName.empty()) {
		options.targetFileName = options.objectFileName;
	}

	verboseOutputConfig(argc, argv);

	if (argc == musl_optind) {
		usage.printAndExit("No input file specified (pass \"-\" to read from standard input)");
	} else if (argc != musl_optind + 1) {
		usage.printAndExit("More than one input file specified");
	}

	std::string mainFileName = argv[musl_optind];

	verbosePrint(VERB_NOTICE, "Assembling \"%s\"\n", mainFileName.c_str()); // LCOV_EXCL_LINE

	if (dependFileName) {
		if (options.targetFileName.empty()) {
			fatal("Dependency files can only be created if a target file is specified with either "
			      "'-o', '-MQ' or '-MT'");
		}

		if (strcmp("-", dependFileName)) {
			options.dependFile = fopen(dependFileName, "w");
			if (options.dependFile == nullptr) {
				// LCOV_EXCL_START
				fatal("Failed to open dependency file \"%s\": %s", dependFileName, strerror(errno));
				// LCOV_EXCL_STOP
			}
		} else {
			options.dependFile = stdout;
		}
	}

	options.printDep(mainFileName);

	charmap_New(DEFAULT_CHARMAP_NAME, nullptr);

	// Init lexer and file stack, providing file info
	fstk_Init(mainFileName);

	// Perform parse (`yy::parser` is auto-generated from `parser.y`)
	if (yy::parser parser; parser.parse() != 0) {
		// Exited due to YYABORT or YYNOMEM
		fatal("Unrecoverable error while parsing"); // LCOV_EXCL_LINE
	}

	// If parse aborted without errors due to a missing INCLUDE, and `-MG` was given, exit normally
	if (fstk_FailedOnMissingInclude()) {
		requireZeroErrors();
		return 0;
	}

	sect_CheckUnionClosed();
	sect_CheckLoadClosed();
	sect_CheckSizes();

	charmap_CheckStack();
	opt_CheckStack();
	sect_CheckStack();

	requireZeroErrors();

	out_WriteObject();

	for (auto const &[name, features] : stateFileSpecs) {
		out_WriteState(name, features);
	}

	return 0;
}
