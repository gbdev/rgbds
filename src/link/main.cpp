// SPDX-License-Identifier: MIT

#include "link/main.hpp"

#include <inttypes.h>
#include <limits.h>
#include <optional>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utility>

#include "backtrace.hpp"
#include "cli.hpp"
#include "diagnostics.hpp"
#include "linkdefs.hpp"
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
#include "link/warning.hpp"

Options options;

// Flags which must be processed after the option parsing finishes
static struct LocalOptions {
	std::optional<std::string> linkerScriptName; // -l
	std::vector<std::string> inputFileNames;     // <file>...
} localOptions;

// Short options
static char const *optstring = "B:dhl:m:Mn:O:o:p:S:tVvW:wx";

// Long-only option variable
static int longOpt; // `--color`

// Equivalent long options
// Please keep in the same order as short opts.
// Also, make sure long opts don't create ambiguity:
// A long opt's name should start with the same letter as its short opt,
// except if it doesn't create any ambiguity (`verbose` versus `version`).
// This is because long opt matching, even to a single char, is prioritized
// over short opt matching.
static option const longopts[] = {
    {"backtrace",     required_argument, nullptr,  'B'},
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
        "[-dhMtVvwx]", "[-B depth]", "[-l script]", "[-m map_file]", "[-n sym_file]",
        "[-O overlay_file]", "[-o out_file]", "[-p pad_value]", "[-S spec]", "<file> ...",
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

static size_t skipBlankSpace(char const *str) {
	return strspn(str, " \t");
}

static void parseScrambleSpec(char *spec) {
	// clang-format off: vertically align nested initializers
	static UpperMap<std::pair<uint16_t *, uint16_t>> scrambleSpecs{
	    {"ROMX",  std::pair{&options.scrambleROMX,  65535}},
	    {"SRAM",  std::pair{&options.scrambleSRAM,  255  }},
	    {"WRAMX", std::pair{&options.scrambleWRAMX, 7    }},
	};
	// clang-format on

	// Skip leading blank space before the regions.
	spec += skipBlankSpace(spec);

	// The argument to `-S` should be a comma-separated list of regions, allowing a trailing comma.
	// Each region name is optionally followed by an '=' and a region size.
	while (*spec) {
		char *regionName = spec;

		// The region name continues (skipping any blank space) until a ',' (next region),
		// '=' (region size), or the end of the string.
		size_t regionNameLen = strcspn(regionName, "=, \t");
		// Skip trailing blank space after the region name.
		size_t regionNameSkipLen = regionNameLen + skipBlankSpace(regionName + regionNameLen);
		spec = regionName + regionNameSkipLen;

		if (*spec != '=' && *spec != ',' && *spec != '\0') {
			fatal("Unexpected character %s in spec for option '-S'", printChar(*spec));
		}

		char *regionSize = nullptr;
		size_t regionSizeLen = 0;
		// The '=' region size limit is optional.
		if (*spec == '=') {
			regionSize = spec + 1; // Skip the '='
			// Skip leading blank space before the region size.
			regionSize += skipBlankSpace(regionSize);

			// The region size continues (skipping any blank space) until a ',' (next region)
			// or the end of the string.
			regionSizeLen = strcspn(regionSize, ", \t");
			// Skip trailing blank space after the region size.
			size_t regionSizeSkipLen = regionSizeLen + skipBlankSpace(regionSize + regionSizeLen);
			spec = regionSize + regionSizeSkipLen;

			if (*spec != ',' && *spec != '\0') {
				fatal("Unexpected character %s in spec for option '-S'", printChar(*spec));
			}
		}

		// Skip trailing comma after the region.
		if (*spec == ',') {
			++spec;
		}
		// Skip trailing blank space after the region.
		// `spec` will be the next region name, or the end of the string.
		spec += skipBlankSpace(spec);

		// Terminate the `regionName` and `regionSize` strings.
		regionName[regionNameLen] = '\0';
		if (regionSize) {
			regionSize[regionSizeLen] = '\0';
		}

		// Check for an empty region name or limit.
		// Note that by skipping leading blank space before the loop, and skipping a trailing comma
		// and blank space before the next iteration, we guarantee that the region name will not be
		// empty if it is present at all.
		if (*regionName == '\0') {
			fatal("Empty region name in spec for option '-S'");
		}
		if (regionSize && *regionSize == '\0') {
			fatal("Empty region size limit in spec for option '-S'");
		}

		// Determine which region type this is.
		auto search = scrambleSpecs.find(regionName);
		if (search == scrambleSpecs.end()) {
			fatal("Unknown region name \"%s\" in spec for option '-S'", regionName);
		}

		uint16_t limit = search->second.second;
		if (regionSize) {
			char const *ptr = regionSize + skipBlankSpace(regionSize);
			if (std::optional<uint64_t> value = parseWholeNumber(ptr); !value) {
				fatal("Invalid region size limit \"%s\" for option '-S'", regionSize);
			} else if (*value > limit) {
				fatal(
				    "%s region size for option '-S' must be between 0 and %" PRIu16,
				    search->first.c_str(),
				    limit
				);
			} else {
				limit = *value;
			}
		} else if (search->second.first != &options.scrambleWRAMX) {
			// Only WRAMX limit can be implied, since ROMX and SRAM size may vary.
			fatal("Missing %s region size limit for option '-S'", search->first.c_str());
		}

		if (*search->second.first != limit && *search->second.first != 0) {
			warnx("Overriding %s region size limit for option '-S'", search->first.c_str());
		}

		// Update the scrambling region size limit.
		*search->second.first = limit;
	}
}

static void parseArg(int ch, char *arg) {
	switch (ch) {
	case 'B':
		if (!trace_ParseTraceDepth(arg)) {
			fatal("Invalid argument for option '-B'");
		}
		break;

	case 'd':
		options.isDmgMode = true;
		options.isWRAM0Mode = true;
		break;

		// LCOV_EXCL_START
	case 'h':
		usage.printAndExit(0);
		// LCOV_EXCL_STOP

	case 'l':
		if (localOptions.linkerScriptName) {
			warnx("Overriding linker script file \"%s\"", localOptions.linkerScriptName->c_str());
		}
		localOptions.linkerScriptName = arg;
		break;

	case 'M':
		options.noSymInMap = true;
		break;

	case 'm':
		if (options.mapFileName) {
			warnx("Overriding map file \"%s\"", options.mapFileName->c_str());
		}
		options.mapFileName = arg;
		break;

	case 'n':
		if (options.symFileName) {
			warnx("Overriding sym file \"%s\"", options.symFileName->c_str());
		}
		options.symFileName = arg;
		break;

	case 'O':
		if (options.overlayFileName) {
			warnx("Overriding overlay file \"%s\"", options.overlayFileName->c_str());
		}
		options.overlayFileName = arg;
		break;

	case 'o':
		if (options.outputFileName) {
			warnx("Overriding output file \"%s\"", options.outputFileName->c_str());
		}
		options.outputFileName = arg;
		break;

	case 'p':
		if (std::optional<uint64_t> value = parseWholeNumber(arg); !value) {
			fatal("Invalid argument for option '-p'");
		} else if (*value > 0xFF) {
			fatal("Argument for option '-p' must be between 0 and 0xFF");
		} else {
			options.padValue = *value;
			options.hasPadValue = true;
		}
		break;

	case 'S':
		parseScrambleSpec(arg);
		break;

	case 't':
		options.is32kMode = true;
		break;

		// LCOV_EXCL_START
	case 'V':
		printf("%s %s\n", usage.name.c_str(), get_package_version_string());
		exit(0);

	case 'v':
		incrementVerbosity();
		break;
		// LCOV_EXCL_STOP

	case 'W':
		warnings.processWarningFlag(arg);
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
		if (longOpt == 'c' && !style_Parse(arg)) {
			fatal("Invalid argument for option '--color'");
		}
		break;

	case 1: // Positional argument
		localOptions.inputFileNames.push_back(arg);
		break;

		// LCOV_EXCL_START
	default:
		usage.printAndExit(1);
		// LCOV_EXCL_STOP
	}
}

// LCOV_EXCL_START
static void verboseOutputConfig() {
	if (!checkVerbosity(VERB_CONFIG)) {
		return;
	}

	style_Set(stderr, STYLE_MAGENTA, false);

	fprintf(stderr, "%s %s\n", usage.name.c_str(), get_package_version_string());

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
	if (!localOptions.inputFileNames.empty()) {
		fprintf(stderr, "\tInput object files: ");
		size_t nbFiles = localOptions.inputFileNames.size();
		for (size_t i = 0; i < nbFiles; ++i) {
			if (i > 0) {
				fputs(", ", stderr);
			}
			if (i == 10) {
				fprintf(stderr, "and %zu more", nbFiles - i);
				break;
			}
			fputs(localOptions.inputFileNames[i].c_str(), stderr);
		}
		putc('\n', stderr);
	}
	auto printPath = [](char const *name, std::optional<std::string> const &path) {
		if (path) {
			fprintf(stderr, "\t%s: %s\n", name, path->c_str());
		}
	};
	// -O/--overlay
	printPath("Overlay file", options.overlayFileName);
	// -l/--linkerscript
	printPath("Linker script", localOptions.linkerScriptName);
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

int main(int argc, char *argv[]) {
	cli_ParseArgs(argc, argv, optstring, longopts, parseArg, usage);

	verboseOutputConfig();

	if (localOptions.inputFileNames.empty()) {
		usage.printAndExit("No input file specified (pass \"-\" to read from standard input)");
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
	size_t nbFiles = localOptions.inputFileNames.size();
	obj_Setup(nbFiles);
	for (size_t i = 0; i < nbFiles; ++i) {
		obj_ReadFile(localOptions.inputFileNames[i], nbFiles - i - 1);
	}

	// apply the linker script's modifications,
	if (localOptions.linkerScriptName) {
		verbosePrint(VERB_NOTICE, "Reading linker script...\n");

		if (lexer_Init(*localOptions.linkerScriptName)) {
			if (yy::parser parser; parser.parse() != 0) {
				// Exited due to YYABORT or YYNOMEM
				fatal("Unrecoverable error while reading linker script"); // LCOV_EXCL_LINE
			}
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

	return 0;
}
