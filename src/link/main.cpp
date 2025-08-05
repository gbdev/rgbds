// SPDX-License-Identifier: MIT

#include <sys/stat.h>
#include <sys/types.h>

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "diagnostics.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp" // assume
#include "itertools.hpp"
#include "platform.hpp"
#include "script.hpp" // Generated from script.y
#include "style.hpp"
#include "usage.hpp"
#include "util.hpp" // UpperMap, printChar
#include "verbosity.hpp"
#include "version.hpp"

#include "link/assign.hpp"
#include "link/lexer.hpp"
#include "link/object.hpp"
#include "link/output.hpp"
#include "link/patch.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"
#include "link/warning.hpp"

Options options;

static char const *linkerScriptName = nullptr; // -l

// Short options
static char const *optstring = "dhl:m:Mn:O:o:p:S:tVvW:wx";

// Variables for the long-only options
static int longOpt; // `--color`

// Equivalent long options
// Please keep in the same order as short opts.
// Also, make sure long opts don't create ambiguity:
// A long opt's name should start with the same letter as its short opt,
// except if it doesn't create any ambiguity (`verbose` versus `version`).
// This is because long opt matching, even to a single char, is prioritized
// over short opt matching.
static option const longopts[] = {
    {"dmg",           no_argument,       nullptr,  'd'},
    {"help",          no_argument,       nullptr,  'h'},
    {"linkerscript",  required_argument, nullptr,  'l'},
    {"map",           required_argument, nullptr,  'm'},
    {"no-sym-in-map", no_argument,       nullptr,  'M'},
    {"sym",           required_argument, nullptr,  'n'},
    {"overlay",       required_argument, nullptr,  'O'},
    {"output",        required_argument, nullptr,  'o'},
    {"pad",           required_argument, nullptr,  'p'},
    {"scramble",      required_argument, nullptr,  'S'},
    {"tiny",          no_argument,       nullptr,  't'},
    {"version",       no_argument,       nullptr,  'V'},
    {"verbose",       no_argument,       nullptr,  'v'},
    {"warning",       required_argument, nullptr,  'W'},
    {"wramx",         no_argument,       nullptr,  'w'},
    {"nopad",         no_argument,       nullptr,  'x'},
    {"color",         required_argument, &longOpt, 'c'},
    {nullptr,         no_argument,       nullptr,  0  },
};

// clang-format off: nested initializers
static Usage usage = {
    .name = "rgblink",
    .flags = {
        "[-dhMtVvwx]", "[-l script]", "[-m map_file]", "[-n sym_file]", "[-O overlay_file]",
        "[-o out_file]", "[-p pad_value]", "[-S spec]", "<file> ...",
    },
    .options = {
        {{"-l", "--linkerscript <path>"}, {"set the input linker script"}},
        {{"-m", "--map <path>"}, {"set the output map file"}},
        {{"-n", "--sym <path>"}, {"set the output symbol list file"}},
        {{"-o", "--output <path>"}, {"set the output file"}},
        {{"-p", "--pad <value>"}, {"set the value to pad between sections with"}},
        {{"-x", "--nopad"}, {"disable padding of output binary"}},
        {{"-V", "--version"}, {"print RGBLINK version and exit"}},
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

	fprintf(stderr, "rgblink %s\n", get_package_version_string());

	printVVVVVVerbosity();

	fputs("Options:\n", stderr);
	// -d/--dmg
	if (options.isDmgMode) {
		fputs("\tDMG mode prohibits non-DMG section types\n", stderr);
	}
	// -t/--tiny
	if (options.is32kMode) {
		fputs("\tROM0 covers the full 32 KiB of ROM\n", stderr);
	}
	// -w/--wramx
	if (options.isWRAM0Mode) {
		fputs("\tWRAM0 covers the full 8 KiB of WRAM\n", stderr);
	}
	// -x/--nopad
	if (options.disablePadding) {
		fputs("\tNo padding at the end of the ROM file\n", stderr);
	}
	// -p/--pad
	fprintf(stderr, "\tPad value: 0x%02" PRIx8 "\n", options.padValue);
	// -S/--scramble
	if (options.scrambleROMX || options.scrambleWRAMX || options.scrambleSRAM) {
		fputs("\tScramble: ", stderr);
		if (options.scrambleROMX) {
			fprintf(stderr, "ROMX = %" PRIu16, options.scrambleROMX);
			if (options.scrambleWRAMX || options.scrambleSRAM) {
				fputs(", ", stderr);
			}
		}
		if (options.scrambleWRAMX) {
			fprintf(stderr, "WRAMX = %" PRIu16, options.scrambleWRAMX);
			if (options.scrambleSRAM) {
				fputs(", ", stderr);
			}
		}
		if (options.scrambleSRAM) {
			fprintf(stderr, "SRAM = %" PRIu16, options.scrambleSRAM);
		}
		putc('\n', stderr);
	}
	// file ...
	if (musl_optind < argc) {
		fprintf(stderr, "\tInput object files: ");
		for (int i = musl_optind; i < argc; ++i) {
			if (i > musl_optind) {
				fputs(", ", stderr);
			}
			if (i - musl_optind == 10) {
				fprintf(stderr, "and %d more", argc - i);
				break;
			}
			fputs(argv[i], stderr);
		}
		putc('\n', stderr);
	}
	auto printPath = [](char const *name, char const *path) {
		if (path) {
			fprintf(stderr, "\t%s: %s\n", name, path);
		}
	};
	// -O/--overlay
	printPath("Overlay file", options.overlayFileName);
	// -l/--linkerscript
	printPath("Linker script", linkerScriptName);
	// -o/--output
	printPath("Output ROM file", options.outputFileName);
	// -m/--map
	printPath("Output map file", options.mapFileName);
	// -M/--no-sym-in-map
	if (options.mapFileName && options.noSymInMap) {
		fputs("\tNo symbols in map file\n", stderr);
	}
	// -n/--sym
	printPath("Output sym file", options.symFileName);
	fputs("Ready.\n", stderr);

	style_Reset(stderr);
}
// LCOV_EXCL_STOP

std::string const &FileStackNode::dump(uint32_t curLineNo) const {
	if (std::holds_alternative<std::vector<uint32_t>>(data)) {
		assume(parent); // REPT nodes use their parent's name
		std::string const &lastName = parent->dump(lineNo);
		style_Set(stderr, STYLE_CYAN, false);
		fputs(" -> ", stderr);
		style_Set(stderr, STYLE_CYAN, true);
		fputs(lastName.c_str(), stderr);
		for (uint32_t iter : iters()) {
			fprintf(stderr, "::REPT~%" PRIu32, iter);
		}
		style_Set(stderr, STYLE_CYAN, false);
		fprintf(stderr, "(%" PRIu32 ")", curLineNo);
		style_Reset(stderr);
		return lastName;
	} else {
		if (parent) {
			parent->dump(lineNo);
			style_Set(stderr, STYLE_CYAN, false);
			fputs(" -> ", stderr);
		}
		std::string const &nodeName = name();
		style_Set(stderr, STYLE_CYAN, true);
		fputs(nodeName.c_str(), stderr);
		style_Set(stderr, STYLE_CYAN, false);
		fprintf(stderr, "(%" PRIu32 ")", curLineNo);
		style_Reset(stderr);
		return nodeName;
	}
}

static void parseScrambleSpec(char *spec) {
	// clang-format off: vertically align nested initializers
	static UpperMap<std::pair<uint16_t *, uint16_t>> scrambleSpecs{
	    {"ROMX",  std::pair{&options.scrambleROMX,  65535}},
	    {"SRAM",  std::pair{&options.scrambleSRAM,  255  }},
	    {"WRAMX", std::pair{&options.scrambleWRAMX, 7    }},
	};
	// clang-format on

	// Skip leading whitespace before the regions.
	spec += strspn(spec, " \t");

	// The argument to `-S` should be a comma-separated list of regions, allowing a trailing comma.
	// Each region name is optionally followed by an '=' and a region size.
	while (*spec) {
		char *regionName = spec;

		// The region name continues (skipping any whitespace) until a ',' (next region),
		// '=' (region size), or the end of the string.
		size_t regionNameLen = strcspn(regionName, "=, \t");
		// Skip trailing whitespace after the region name.
		size_t regionNameSkipLen = regionNameLen + strspn(regionName + regionNameLen, " \t");
		spec = regionName + regionNameSkipLen;

		if (*spec != '=' && *spec != ',' && *spec != '\0') {
			fatal("Unexpected character %s in spec for option 'S'", printChar(*spec));
		}

		char *regionSize = nullptr;
		size_t regionSizeLen = 0;
		// The '=' region size limit is optional.
		if (*spec == '=') {
			regionSize = spec + 1; // Skip the '='
			// Skip leading whitespace before the region size.
			regionSize += strspn(regionSize, " \t");

			// The region size continues (skipping any whitespace) until a ',' (next region)
			// or the end of the string.
			regionSizeLen = strcspn(regionSize, ", \t");
			// Skip trailing whitespace after the region size.
			size_t regionSizeSkipLen = regionSizeLen + strspn(regionSize + regionSizeLen, " \t");
			spec = regionSize + regionSizeSkipLen;

			if (*spec != ',' && *spec != '\0') {
				fatal("Unexpected character %s in spec for option 'S'", printChar(*spec));
			}
		}

		// Skip trailing comma after the region.
		if (*spec == ',') {
			++spec;
		}
		// Skip trailing whitespace after the region.
		// `spec` will be the next region name, or the end of the string.
		spec += strspn(spec, " \t");

		// Terminate the `regionName` and `regionSize` strings.
		regionName[regionNameLen] = '\0';
		if (regionSize) {
			regionSize[regionSizeLen] = '\0';
		}

		// Check for an empty region name or limit.
		// Note that by skipping leading whitespace before the loop, and skipping a trailing comma
		// and whitespace before the next iteration, we guarantee that the region name will not be
		// empty if it is present at all.
		if (*regionName == '\0') {
			fatal("Empty region name in spec for option 'S'");
		}
		if (regionSize && *regionSize == '\0') {
			fatal("Empty region size limit in spec for option 'S'");
		}

		// Determine which region type this is.
		auto search = scrambleSpecs.find(regionName);
		if (search == scrambleSpecs.end()) {
			fatal("Unknown region name \"%s\" in spec for option 'S'", regionName);
		}

		uint16_t limit = search->second.second;
		if (regionSize) {
			char *endptr;
			unsigned long value = strtoul(regionSize, &endptr, 0);

			if (*endptr != '\0') {
				fatal("Invalid region size limit \"%s\" for option 'S'", regionSize);
			}
			if (value > limit) {
				fatal(
				    "%s region size for option 'S' must be between 0 and %" PRIu16,
				    search->first.c_str(),
				    limit
				);
			}

			limit = value;
		} else if (search->second.first != &options.scrambleWRAMX) {
			// Only WRAMX limit can be implied, since ROMX and SRAM size may vary.
			fatal("Missing %s region size limit for option 'S'", search->first.c_str());
		}

		if (*search->second.first != limit && *search->second.first != 0) {
			warnx("Overriding %s region size limit for option 'S'", search->first.c_str());
		}

		// Update the scrambling region size limit.
		*search->second.first = limit;
	}
}

int main(int argc, char *argv[]) {
	// Parse options
	for (int ch; (ch = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1;) {
		switch (ch) {
		case 'd':
			options.isDmgMode = true;
			options.isWRAM0Mode = true;
			break;
		case 'h':
			usage.printAndExit(0); // LCOV_EXCL_LINE
		case 'l':
			if (linkerScriptName) {
				warnx("Overriding linker script %s", linkerScriptName);
			}
			linkerScriptName = musl_optarg;
			break;
		case 'M':
			options.noSymInMap = true;
			break;
		case 'm':
			if (options.mapFileName) {
				warnx("Overriding map file %s", options.mapFileName);
			}
			options.mapFileName = musl_optarg;
			break;
		case 'n':
			if (options.symFileName) {
				warnx("Overriding sym file %s", options.symFileName);
			}
			options.symFileName = musl_optarg;
			break;
		case 'O':
			if (options.overlayFileName) {
				warnx("Overriding overlay file %s", options.overlayFileName);
			}
			options.overlayFileName = musl_optarg;
			break;
		case 'o':
			if (options.outputFileName) {
				warnx("Overriding output file %s", options.outputFileName);
			}
			options.outputFileName = musl_optarg;
			break;
		case 'p': {
			char *endptr;
			unsigned long value = strtoul(musl_optarg, &endptr, 0);

			if (musl_optarg[0] == '\0' || *endptr != '\0') {
				fatal("Invalid argument for option 'p'");
			}
			if (value > 0xFF) {
				fatal("Argument for option 'p' must be between 0 and 0xFF");
			}

			options.padValue = value;
			options.hasPadValue = true;
			break;
		}
		case 'S':
			parseScrambleSpec(musl_optarg);
			break;
		case 't':
			options.is32kMode = true;
			break;
		case 'V':
			// LCOV_EXCL_START
			printf("rgblink %s\n", get_package_version_string());
			exit(0);
			// LCOV_EXCL_STOP
		case 'v':
			// LCOV_EXCL_START
			incrementVerbosity();
			break;
			// LCOV_EXCL_STOP
		case 'W':
			warnings.processWarningFlag(musl_optarg);
			break;
		case 'w':
			options.isWRAM0Mode = true;
			break;
		case 'x':
			options.disablePadding = true;
			// implies tiny mode
			options.is32kMode = true;
			break;
		case 0: // Long-only options
			if (longOpt == 'c') {
				if (!strcasecmp(musl_optarg, "always")) {
					style_Enable(true);
				} else if (!strcasecmp(musl_optarg, "never")) {
					style_Enable(false);
				} else if (strcasecmp(musl_optarg, "auto")) {
					fatal("Invalid argument for option '--color'");
				}
			}
			break;
		default:
			usage.printAndExit(1); // LCOV_EXCL_LINE
		}
	}

	verboseOutputConfig(argc, argv);

	// If no input files were specified, the user must have screwed up
	if (musl_optind == argc) {
		usage.printAndExit("Please specify an input file (pass `-` to read from standard input)");
	}

	// Patch the size array depending on command-line options
	if (!options.is32kMode) {
		sectionTypeInfo[SECTTYPE_ROM0].size = 0x4000;
	}
	if (!options.isWRAM0Mode) {
		sectionTypeInfo[SECTTYPE_WRAM0].size = 0x1000;
	}

	// Patch the bank ranges array depending on command-line options
	if (options.isDmgMode) {
		sectionTypeInfo[SECTTYPE_VRAM].lastBank = 0;
	}

	// Read all object files first,
	obj_Setup(argc - musl_optind);
	for (int i = musl_optind; i < argc; ++i) {
		obj_ReadFile(argv[i], argc - i - 1);
	}

	// apply the linker script's modifications,
	if (linkerScriptName) {
		verbosePrint(VERB_NOTICE, "Reading linker script...\n");

		if (lexer_Init(linkerScriptName)) {
			yy::parser parser;
			// We don't care about the return value, as any error increments the global error count,
			// which is what `main` checks.
			(void)parser.parse();
		}

		// If the linker script produced any errors, some sections may be in an invalid state
		requireZeroErrors();
	}

	// then process them,
	sect_DoSanityChecks();
	requireZeroErrors();
	assign_AssignSections();
	patch_CheckAssertions();

	// and finally output the result.
	patch_ApplyPatches();
	requireZeroErrors();
	out_WriteFiles();
}
