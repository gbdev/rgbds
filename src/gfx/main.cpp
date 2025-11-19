// SPDX-License-Identifier: MIT

#include "gfx/main.hpp"

#include <inttypes.h>
#include <ios>
#include <optional>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <vector>

#include "cli.hpp"
#include "diagnostics.hpp"
#include "file.hpp"
#include "helpers.hpp"
#include "platform.hpp"
#include "style.hpp"
#include "usage.hpp"
#include "util.hpp"
#include "verbosity.hpp"
#include "version.hpp"

#include "gfx/pal_spec.hpp"
#include "gfx/process.hpp"
#include "gfx/reverse.hpp"
#include "gfx/rgba.hpp"
#include "gfx/warning.hpp"

using namespace std::literals::string_view_literals;

Options options;

// Flags which must be processed after the option parsing finishes
static struct LocalOptions {
	std::optional<std::string> externalPalSpec; // -c
	bool autoAttrmap;                           // -A
	bool autoTilemap;                           // -T
	bool autoPalettes;                          // -P
	bool autoPalmap;                            // -Q
	bool groupOutputs;                          // -O
	bool reverse;                               // -r

	bool autoAny() const { return autoAttrmap || autoTilemap || autoPalettes || autoPalmap; }
} localOptions;

// Short options
static char const *optstring = "Aa:B:b:Cc:d:hi:L:l:mN:n:Oo:Pp:Qq:r:s:Tt:U:uVvW:wXx:YZ";

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
    {"auto-attr-map",    no_argument,       nullptr,  'A'},
    {"attr-map",         required_argument, nullptr,  'a'},
    {"background-color", required_argument, nullptr,  'B'},
    {"base-tiles",       required_argument, nullptr,  'b'},
    {"color-curve",      no_argument,       nullptr,  'C'},
    {"colors",           required_argument, nullptr,  'c'},
    {"depth",            required_argument, nullptr,  'd'},
    {"help",             no_argument,       nullptr,  'h'},
    {"input-tileset",    required_argument, nullptr,  'i'},
    {"slice",            required_argument, nullptr,  'L'},
    {"base-palette",     required_argument, nullptr,  'l'},
    {"mirror-tiles",     no_argument,       nullptr,  'm'},
    {"nb-tiles",         required_argument, nullptr,  'N'},
    {"nb-palettes",      required_argument, nullptr,  'n'},
    {"group-outputs",    no_argument,       nullptr,  'O'},
    {"output",           required_argument, nullptr,  'o'},
    {"auto-palette",     no_argument,       nullptr,  'P'},
    {"palette",          required_argument, nullptr,  'p'},
    {"auto-palette-map", no_argument,       nullptr,  'Q'},
    {"palette-map",      required_argument, nullptr,  'q'},
    {"reverse",          required_argument, nullptr,  'r'},
    {"palette-size",     required_argument, nullptr,  's'},
    {"auto-tilemap",     no_argument,       nullptr,  'T'},
    {"tilemap",          required_argument, nullptr,  't'},
    {"unit-size",        required_argument, nullptr,  'U'},
    {"unique-tiles",     no_argument,       nullptr,  'u'},
    {"version",          no_argument,       nullptr,  'V'},
    {"verbose",          no_argument,       nullptr,  'v'},
    {"warning",          required_argument, nullptr,  'W'},
    {"mirror-x",         no_argument,       nullptr,  'X'},
    {"trim-end",         required_argument, nullptr,  'x'},
    {"mirror-y",         no_argument,       nullptr,  'Y'},
    {"columns",          no_argument,       nullptr,  'Z'},
    {"color",            required_argument, &longOpt, 'c'},
    {nullptr,            no_argument,       nullptr,  0  },
};

// clang-format off: nested initializers
static Usage usage = {
    .name = "rgbgfx",
    .flags = {
        "[-r stride]", "[-ChmOuVXYZ]", "[-v [-v ...]]", "[-a <attr_map> | -A]", "[-b <base_ids>]",
        "[-c <colors>]", "[-d <depth>]", "[-i <tileset_file>]", "[-L <slice>]", "[-l <base_pal>]",
        "[-N <nb_tiles>]", "[-n <nb_pals>]", "[-o <out_file>]", "[-p <pal_file> | -P]",
        "[-q <pal_map> | -Q]", "[-s <nb_colors>]", "[-t <tile_map> | -T]", "[-x <nb_tiles>]",
        "<file>",
    },
    .options = {
        {{"-m", "--mirror-tiles"}, {"optimize out mirrored tiles"}},
        {{"-o", "--output <path>"}, {"output the tile data to this path"}},
        {{"-t", "--tilemap <path>"}, {"output the tile map to this path"}},
        {{"-u", "--unique-tiles"}, {"optimize out identical tiles"}},
        {{"-V", "--version"}, {"print RGBGFX version and exit"}},
        {{"-W", "--warning <warning>"}, {"enable or disable warnings"}},
    },
};
// clang-format on

// Parses a number at the beginning of a string, moving the pointer to skip the parsed characters.
// Returns the provided errVal on error.
static uint16_t readNumber(char const *&str, char const *errPrefix, uint16_t errVal = UINT16_MAX) {
	if (std::optional<uint64_t> number = parseNumber(str); !number) {
		error("%s: expected number, but found nothing", errPrefix);
		return errVal;
	} else if (*number > UINT16_MAX) {
		error("%s: the number is too large!", errPrefix);
		return errVal;
	} else {
		return *number;
	}
}

static void skipBlankSpace(char const *&arg) {
	arg += strspn(arg, " \t");
}

static void parseArg(int ch, char *arg) {
	char const *argPtr = arg; // Make a copy for scanning

	switch (ch) {
	case 'A':
		localOptions.autoAttrmap = true;
		break;

	case 'a':
		localOptions.autoAttrmap = false;
		if (!options.attrmap.empty()) {
			warnx("Overriding attrmap file \"%s\"", options.attrmap.c_str());
		}
		options.attrmap = arg;
		break;

	case 'B':
		parseBackgroundPalSpec(arg);
		break;

	case 'b': {
		uint16_t number = readNumber(argPtr, "Bank 0 base tile ID", 0);
		if (number >= 256) {
			error("Bank 0 base tile ID must be below 256");
		} else {
			options.baseTileIDs[0] = number;
		}
		if (*argPtr == '\0') {
			options.baseTileIDs[1] = 0;
			break;
		}
		skipBlankSpace(argPtr);
		if (*argPtr != ',') {
			error("Base tile IDs must be one or two comma-separated numbers, not \"%s\"", arg);
			break;
		}
		++argPtr; // Skip comma
		skipBlankSpace(argPtr);
		number = readNumber(argPtr, "Bank 1 base tile ID", 0);
		if (number >= 256) {
			error("Bank 1 base tile ID must be below 256");
		} else {
			options.baseTileIDs[1] = number;
		}
		if (*argPtr != '\0') {
			error("Base tile IDs must be one or two comma-separated numbers, not \"%s\"", arg);
			break;
		}
		break;
	}

	case 'C':
		options.useColorCurve = true;
		break;

	case 'c':
		localOptions.externalPalSpec = std::nullopt; // Allow overriding a previous pal spec
		if (arg[0] == '#') {
			options.palSpecType = Options::EXPLICIT;
			parseInlinePalSpec(arg);
		} else if (strcasecmp(arg, "embedded") == 0) {
			// Use PLTE, error out if missing
			options.palSpecType = Options::EMBEDDED;
		} else if (strcasecmp(arg, "auto") == 0) {
			options.palSpecType = Options::NO_SPEC;
		} else if (strcasecmp(arg, "dmg") == 0) {
			options.palSpecType = Options::DMG;
			parseDmgPalSpec(0xE4); // Same darkest-first order as `sortGrayscale`
		} else if (strncasecmp(arg, "dmg=", literal_strlen("dmg=")) == 0) {
			options.palSpecType = Options::DMG;
			parseDmgPalSpec(&arg[literal_strlen("dmg=")]);
		} else {
			options.palSpecType = Options::EXPLICIT;
			localOptions.externalPalSpec = arg;
		}
		break;

	case 'd':
		options.bitDepth = readNumber(argPtr, "Bit depth", 2);
		if (*argPtr != '\0') {
			error("Bit depth ('-b') argument must be a valid number, not \"%s\"", arg);
		} else if (options.bitDepth != 1 && options.bitDepth != 2) {
			error("Bit depth must be 1 or 2, not %" PRIu8, options.bitDepth);
			options.bitDepth = 2;
		}
		break;

		// LCOV_EXCL_START
	case 'h':
		usage.printAndExit(0);
		// LCOV_EXCL_STOP

	case 'i':
		if (!options.inputTileset.empty()) {
			warnx("Overriding input tileset file \"%s\"", options.inputTileset.c_str());
		}
		options.inputTileset = arg;
		break;

	case 'L':
		options.inputSlice.left = readNumber(argPtr, "Input slice left coordinate");
		if (options.inputSlice.left > INT16_MAX) {
			error("Input slice left coordinate is out of range!");
			break;
		}
		skipBlankSpace(argPtr);
		if (*argPtr != ',') {
			error("Missing comma after left coordinate in \"%s\"", arg);
			break;
		}
		++argPtr;
		skipBlankSpace(argPtr);
		options.inputSlice.top = readNumber(argPtr, "Input slice upper coordinate");
		skipBlankSpace(argPtr);
		if (*argPtr != ':') {
			error("Missing colon after upper coordinate in \"%s\"", arg);
			break;
		}
		++argPtr;
		skipBlankSpace(argPtr);
		options.inputSlice.width = readNumber(argPtr, "Input slice width");
		skipBlankSpace(argPtr);
		if (options.inputSlice.width == 0) {
			error("Input slice width may not be 0!");
		}
		if (*argPtr != ',') {
			error("Missing comma after width in \"%s\"", arg);
			break;
		}
		++argPtr;
		skipBlankSpace(argPtr);
		options.inputSlice.height = readNumber(argPtr, "Input slice height");
		if (options.inputSlice.height == 0) {
			error("Input slice height may not be 0!");
		}
		if (*argPtr != '\0') {
			error("Unexpected extra characters after slice spec in \"%s\"", arg);
		}
		break;

	case 'l': {
		uint16_t number = readNumber(argPtr, "Base palette ID", 0);
		if (*argPtr != '\0') {
			error("Base palette ID must be a valid number, not \"%s\"", arg);
		} else if (number >= 256) {
			error("Base palette ID must be below 256");
		} else {
			options.basePalID = number;
		}
		break;
	}

	case 'm':
		options.allowMirroringX = true; // Imply `-X`
		options.allowMirroringY = true; // Imply `-Y`
		[[fallthrough]];                // Imply `-u`

	case 'u':
		options.allowDedup = true;
		break;

	case 'N':
		options.maxNbTiles[0] = readNumber(argPtr, "Number of tiles in bank 0", 256);
		if (options.maxNbTiles[0] > 256) {
			error("Bank 0 cannot contain more than 256 tiles");
		}
		if (*argPtr == '\0') {
			options.maxNbTiles[1] = 0;
			break;
		}
		skipBlankSpace(argPtr);
		if (*argPtr != ',') {
			error("Bank capacity must be one or two comma-separated numbers, not \"%s\"", arg);
			break;
		}
		++argPtr; // Skip comma
		skipBlankSpace(argPtr);
		options.maxNbTiles[1] = readNumber(argPtr, "Number of tiles in bank 1", 256);
		if (options.maxNbTiles[1] > 256) {
			error("Bank 1 cannot contain more than 256 tiles");
		}
		if (*argPtr != '\0') {
			error("Bank capacity must be one or two comma-separated numbers, not \"%s\"", arg);
			break;
		}
		break;

	case 'n': {
		uint16_t number = readNumber(argPtr, "Number of palettes", 256);
		if (*argPtr != '\0') {
			error("Number of palettes ('-n') must be a valid number, not \"%s\"", arg);
		}
		if (number > 256) {
			error("Number of palettes ('-n') must not exceed 256!");
		} else if (number == 0) {
			error("Number of palettes ('-n') may not be 0!");
		} else {
			options.nbPalettes = number;
		}
		break;
	}

	case 'O':
		localOptions.groupOutputs = true;
		break;

	case 'o':
		if (!options.output.empty()) {
			warnx("Overriding tile data file %s", options.output.c_str());
		}
		options.output = arg;
		break;

	case 'P':
		localOptions.autoPalettes = true;
		break;

	case 'p':
		localOptions.autoPalettes = false;
		if (!options.palettes.empty()) {
			warnx("Overriding palettes file %s", options.palettes.c_str());
		}
		options.palettes = arg;
		break;

	case 'Q':
		localOptions.autoPalmap = true;
		break;

	case 'q':
		localOptions.autoPalmap = false;
		if (!options.palmap.empty()) {
			warnx("Overriding palette map file %s", options.palmap.c_str());
		}
		options.palmap = arg;
		break;

	case 'r':
		localOptions.reverse = true;
		options.reversedWidth = readNumber(argPtr, "Reversed image stride");
		if (*argPtr != '\0') {
			error("Reversed image stride ('-r') must be a valid number, not \"%s\"", arg);
		}
		break;

	case 's':
		options.nbColorsPerPal = readNumber(argPtr, "Number of colors per palette", 4);
		if (*argPtr != '\0') {
			error("Palette size ('-s') must be a valid number, not \"%s\"", arg);
		}
		if (options.nbColorsPerPal > 4) {
			error("Palette size ('-s') must not exceed 4!");
		} else if (options.nbColorsPerPal == 0) {
			error("Palette size ('-s') may not be 0!");
		}
		break;

	case 'T':
		localOptions.autoTilemap = true;
		break;

	case 't':
		localOptions.autoTilemap = false;
		if (!options.tilemap.empty()) {
			warnx("Overriding tilemap file %s", options.tilemap.c_str());
		}
		options.tilemap = arg;
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
		warnings.state.warningsEnabled = false;
		break;

	case 'x':
		options.trim = readNumber(argPtr, "Number of tiles to trim", 0);
		if (*argPtr != '\0') {
			error("Tile trim ('-x') argument must be a valid number, not \"%s\"", arg);
		}
		break;

	case 'X':
		options.allowMirroringX = true;
		options.allowDedup = true; // Imply `-u`
		break;

	case 'Y':
		options.allowMirroringY = true;
		options.allowDedup = true; // Imply `-u`
		break;

	case 'Z':
		options.columnMajor = true;
		break;

	case 0: // Long-only options
		if (longOpt == 'c' && !style_Parse(arg)) {
			fatal("Invalid argument for option '--color'");
		}
		break;

	case 1: // Positional argument
		if (!options.input.empty()) {
			usage.printAndExit(
			    "Input image specified more than once! (first \"%s\", then \"%s\")",
			    options.input.c_str(),
			    arg
			);
		} else if (arg[0] == '\0') { // Empty input path
			usage.printAndExit("Input image path cannot be empty");
		} else {
			options.input = arg;
		}
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
	// -Z/--columns
	if (options.columnMajor) {
		fputs("\tVisit image in column-major order\n", stderr);
	}
	// -u/--unique-tiles
	if (options.allowDedup) {
		fputs("\tAllow deduplicating identical tiles\n", stderr);
	}
	// -m/--mirror-tiles
	if (options.allowMirroringX && options.allowMirroringY) {
		fputs("\tAllow deduplicating mirrored tiles\n", stderr);
	}
	// -X/--mirror-x
	else if (options.allowMirroringX) {
		fputs("\tAllow deduplicating horizontally mirrored tiles\n", stderr);
	}
	// -Y/--mirror-y
	else if (options.allowMirroringY) {
		fputs("\tAllow deduplicating vertically mirrored tiles\n", stderr);
	}
	// -C/--color-curve
	if (options.useColorCurve) {
		fputs("\tUse color curve\n", stderr);
	}
	// -d/--depth
	fprintf(stderr, "\tBit depth: %" PRIu8 "bpp\n", options.bitDepth);
	// -x/--trim-end
	if (options.trim != 0) {
		fprintf(stderr, "\tTrim the last %" PRIu64 " tiles\n", options.trim);
	}
	// -n/--nb-palettes
	fprintf(stderr, "\tMaximum %" PRIu16 " palettes\n", options.nbPalettes);
	// -s/--palette-size
	fprintf(stderr, "\tPalettes contain %" PRIu8 " colors\n", options.nbColorsPerPal);
	// -c/--colors
	if (options.palSpecType == Options::NO_SPEC) {
		fputs("\tAutomatic palette generation\n", stderr);
	} else {
		fprintf(stderr, "\t%s palette spec\n", [] {
			switch (options.palSpecType) {
			case Options::EXPLICIT:
				return "Explicit";
			case Options::EMBEDDED:
				return "Embedded";
			case Options::DMG:
				return "DMG";
			default:
				return "???";
			}
		}());
	}
	if (options.palSpecType == Options::EXPLICIT) {
		fputs("\t[\n", stderr);
		for (auto const &pal : options.palSpec) {
			fputs("\t\t", stderr);
			for (auto const &color : pal) {
				if (color) {
					fprintf(stderr, "#%06x, ", color->toCSS() >> 8);
				} else {
					fputs("#none, ", stderr);
				}
			}
			putc('\n', stderr);
		}
		fputs("\t]\n", stderr);
	}
	// -L/--slice
	if (options.inputSlice.width || options.inputSlice.height || options.inputSlice.left
	    || options.inputSlice.top) {
		fprintf(
		    stderr,
		    "\tInput image slice: %" PRIu16 "x%" PRIu16 " pixels starting at (%" PRIu16 ", %" PRIu16
		    ")\n",
		    options.inputSlice.width,
		    options.inputSlice.height,
		    options.inputSlice.left,
		    options.inputSlice.top
		);
	}
	// -b/--base-tiles
	if (options.baseTileIDs[0] || options.baseTileIDs[1]) {
		fprintf(
		    stderr,
		    "\tBase tile IDs: bank 0 = 0x%02" PRIx8 ", bank 1 = 0x%02" PRIx8 "\n",
		    options.baseTileIDs[0],
		    options.baseTileIDs[1]
		);
	}
	// -l/--base-palette
	if (options.basePalID) {
		fprintf(stderr, "\tBase palette ID: %" PRIu8 "\n", options.basePalID);
	}
	// -N/--nb-tiles
	fprintf(
	    stderr,
	    "\tMaximum %" PRIu16 " tiles in bank 0, and %" PRIu16 " in bank 1\n",
	    options.maxNbTiles[0],
	    options.maxNbTiles[1]
	);
	// -O/--group-outputs (influences other options)
	auto printPath = [](char const *name, std::string const &path) {
		if (!path.empty()) {
			fprintf(stderr, "\t%s: %s\n", name, path.c_str());
		}
	};
	// file
	printPath("Input image", options.input);
	// -i/--input-tileset
	printPath("Input tileset", options.inputTileset);
	// -o/--output
	printPath("Output tile data", options.output);
	// -t/--tilemap or -T/--auto-tilemap
	printPath("Output tilemap", options.tilemap);
	// -a/--attrmap or -A/--auto-attrmap
	printPath("Output attrmap", options.attrmap);
	// -p/--palette or -P/--auto-palette
	printPath("Output palettes", options.palettes);
	// -q/--palette-map or -Q/--auto-palette-map
	printPath("Output palette map", options.palmap);
	// -r/--reverse
	if (localOptions.reverse) {
		fprintf(stderr, "\tReverse image width: %" PRIu16 " tiles\n", options.reversedWidth);
	}
	fputs("Ready.\n", stderr);

	style_Reset(stderr);
}
// LCOV_EXCL_STOP

// Manual implementation of std::filesystem::path.replace_extension().
// macOS <10.15 did not support std::filesystem::path.
static void replaceExtension(std::string &path, char const *extension) {
	constexpr std::string_view chars =
// Both must start with a dot!
#if defined(_MSC_VER) || defined(__MINGW32__)
	    "./\\"sv;
#else
	    "./"sv;
#endif
	size_t len = path.npos;
	if (size_t i = path.find_last_of(chars); i != path.npos && path[i] == '.') {
		// We found the last dot, but check if it's part of a stem
		// (There must be a non-path separator character before it)
		if (i != 0 && chars.find(path[i - 1], 1) == chars.npos) {
			// We can replace the extension
			len = i;
		}
	}
	path.assign(path, 0, len);
	path.append(extension);
}

int main(int argc, char *argv[]) {
	cli_ParseArgs(argc, argv, optstring, longopts, parseArg, usage);

	if (options.nbColorsPerPal == 0) {
		options.nbColorsPerPal = 1u << options.bitDepth;
	} else if (options.nbColorsPerPal > 1u << options.bitDepth) {
		error(
		    "%" PRIu8 "bpp palettes can only contain %u colors, not %" PRIu8,
		    options.bitDepth,
		    1u << options.bitDepth,
		    options.nbColorsPerPal
		);
	}

	if (localOptions.groupOutputs) {
		if (!localOptions.autoAny()) {
			warnx("Grouping outputs ('-O') is enabled, but without any automatic output paths "
			      "('-A', '-P', '-Q', or '-T')");
		}
		if (options.output.empty()) {
			warnx("Grouping outputs ('-O') is enabled, but without an output tile data file ('-o')"
			);
		}
	}
	auto autoOutPath = [](bool autoOptEnabled, std::string &path, char const *extension) {
		if (!autoOptEnabled) {
			return;
		}
		path = localOptions.groupOutputs ? options.output : options.input;
		if (path.empty()) {
			usage.printAndExit(
			    "No %s specified",
			    localOptions.groupOutputs ? "output tile data file" : "input image"
			);
		}
		replaceExtension(path, extension);
	};
	autoOutPath(localOptions.autoAttrmap, options.attrmap, ".attrmap");
	autoOutPath(localOptions.autoTilemap, options.tilemap, ".tilemap");
	autoOutPath(localOptions.autoPalettes, options.palettes, ".pal");
	autoOutPath(localOptions.autoPalmap, options.palmap, ".palmap");

	// Execute deferred external pal spec parsing, now that all other params are known
	if (localOptions.externalPalSpec) {
		parseExternalPalSpec(localOptions.externalPalSpec->c_str());
	}

	verboseOutputConfig(); // LCOV_EXCL_LINE

	// Do not do anything if option parsing went wrong.
	requireZeroErrors();

	if (!options.input.empty()) {
		if (localOptions.reverse) {
			reverse();
		} else {
			process();
		}
	} else if (!options.palettes.empty() && options.palSpecType == Options::EXPLICIT
	           && !localOptions.reverse) {
		processPalettes();
	} else {
		usage.printAndExit("No input file specified (pass \"-\" to read from standard input)");
	}

	requireZeroErrors();
	return 0;
}
