// SPDX-License-Identifier: MIT

#include <sys/stat.h>
#include <sys/types.h>

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp" // assume
#include "itertools.hpp"
#include "platform.hpp"
#include "script.hpp"
#include "version.hpp"

#include "link/assign.hpp"
#include "link/object.hpp"
#include "link/output.hpp"
#include "link/patch.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"
#include "link/warning.hpp"

Options options;

std::string const &FileStackNode::dump(uint32_t curLineNo) const {
	if (std::holds_alternative<std::vector<uint32_t>>(data)) {
		assume(parent); // REPT nodes use their parent's name
		std::string const &lastName = parent->dump(lineNo);
		fputs(" -> ", stderr);
		fputs(lastName.c_str(), stderr);
		for (uint32_t iter : iters()) {
			fprintf(stderr, "::REPT~%" PRIu32, iter);
		}
		fprintf(stderr, "(%" PRIu32 ")", curLineNo);
		return lastName;
	} else {
		if (parent) {
			parent->dump(lineNo);
			fputs(" -> ", stderr);
		}
		std::string const &nodeName = name();
		fputs(nodeName.c_str(), stderr);
		fprintf(stderr, "(%" PRIu32 ")", curLineNo);
		return nodeName;
	}
}

// Short options
static char const *optstring = "dhl:m:Mn:O:o:p:S:tVvW:wx";

// Equivalent long options
// Please keep in the same order as short opts.
// Also, make sure long opts don't create ambiguity:
// A long opt's name should start with the same letter as its short opt,
// except if it doesn't create any ambiguity (`verbose` versus `version`).
// This is because long opt matching, even to a single char, is prioritized
// over short opt matching.
static option const longopts[] = {
    {"dmg",           no_argument,       nullptr, 'd'},
    {"help",          no_argument,       nullptr, 'h'},
    {"linkerscript",  required_argument, nullptr, 'l'},
    {"map",           required_argument, nullptr, 'm'},
    {"no-sym-in-map", no_argument,       nullptr, 'M'},
    {"sym",           required_argument, nullptr, 'n'},
    {"overlay",       required_argument, nullptr, 'O'},
    {"output",        required_argument, nullptr, 'o'},
    {"pad",           required_argument, nullptr, 'p'},
    {"scramble",      required_argument, nullptr, 'S'},
    {"tiny",          no_argument,       nullptr, 't'},
    {"version",       no_argument,       nullptr, 'V'},
    {"verbose",       no_argument,       nullptr, 'v'},
    {"warning",       required_argument, nullptr, 'W'},
    {"wramx",         no_argument,       nullptr, 'w'},
    {"nopad",         no_argument,       nullptr, 'x'},
    {nullptr,         no_argument,       nullptr, 0  }
};

// LCOV_EXCL_START
static void printUsage() {
	fputs(
	    "Usage: rgblink [-dhMtVvwx] [-l script] [-m map_file] [-n sym_file]\n"
	    "               [-O overlay_file] [-o out_file] [-p pad_value]\n"
	    "               [-S spec] <file> ...\n"
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
	    stderr
	);
}
// LCOV_EXCL_STOP

[[gnu::format(printf, 1, 2), noreturn]]
static void fatalWithUsage(char const *fmt, ...) {
	va_list ap;
	fputs("FATAL: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	printUsage();
	exit(1);
}

enum ScrambledRegion {
	SCRAMBLE_ROMX,
	SCRAMBLE_SRAM,
	SCRAMBLE_WRAMX,

	SCRAMBLE_UNK, // Used for errors
};

struct {
	char const *name;
	uint16_t max;
} scrambleSpecs[SCRAMBLE_UNK] = {
    {"romx",  65535}, // SCRAMBLE_ROMX
    {"sram",  255  }, // SCRAMBLE_SRAM
    {"wramx", 7    }, // SCRAMBLE_WRAMX
};

static void parseScrambleSpec(char const *spec) {
	// Skip any leading whitespace
	spec += strspn(spec, " \t");

	// The argument to `-S` should be a comma-separated list of sections followed by an '='
	// indicating their scramble limit.
	while (spec) {
		// Invariant: we should not be pointing at whitespace at this point
		assume(*spec != ' ' && *spec != '\t');

		// Remember where the region's name begins and ends
		char const *regionName = spec;
		size_t regionNameLen = strcspn(spec, "=, \t");
		// Length of region name string slice for print formatting, truncated if too long
		int regionNameFmtLen = regionNameLen > INT_MAX ? INT_MAX : static_cast<int>(regionNameLen);
		ScrambledRegion region = SCRAMBLE_UNK;

		// If this trips, `spec` must be pointing at a ',' or '=' (or NUL) due to the assumption
		if (regionNameLen == 0) {
			argErr('S', "Missing region name");

			if (*spec == '\0') {
				break;
			}
			if (*spec == '=') {               // Skip the limit, too
				spec = strchr(&spec[1], ','); // Skip to next comma, if any
			}
			goto next;
		}

		// Find the next non-blank char after the region name's end
		spec += regionNameLen + strspn(&spec[regionNameLen], " \t");
		if (*spec != '\0' && *spec != ',' && *spec != '=') {
			argErr(
			    'S',
			    "Unexpected '%c' after region name \"%.*s\"",
			    *spec,
			    regionNameFmtLen,
			    regionName
			);
			// Skip to next ',' or '=' (or NUL) and keep parsing
			spec += 1 + strcspn(&spec[1], ",=");
		}

		// Now, determine which region type this is
		for (ScrambledRegion r : EnumSeq(SCRAMBLE_UNK)) {
			// If the strings match (case-insensitively), we got it!
			// `strncasecmp` must be used here since `regionName` points
			// to the entire remaining argument.
			if (!strncasecmp(scrambleSpecs[r].name, regionName, regionNameLen)) {
				region = r;
				break;
			}
		}

		if (region == SCRAMBLE_UNK) {
			argErr('S', "Unknown region \"%.*s\"", regionNameFmtLen, regionName);
		}

		if (*spec == '=') {
			++spec; // `strtoul` will skip the whitespace on its own
			unsigned long limit;
			char *endptr;

			if (*spec == '\0' || *spec == ',') {
				argErr('S', "Empty limit for region \"%.*s\"", regionNameFmtLen, regionName);
				goto next;
			}
			limit = strtoul(spec, &endptr, 10);
			endptr += strspn(endptr, " \t");
			if (*endptr != '\0' && *endptr != ',') {
				argErr(
				    'S',
				    "Invalid non-numeric limit for region \"%.*s\"",
				    regionNameFmtLen,
				    regionName
				);
				endptr = strchr(endptr, ',');
			}
			spec = endptr;

			if (region != SCRAMBLE_UNK && limit > scrambleSpecs[region].max) {
				argErr(
				    'S',
				    "Limit for region \"%.*s\" may not exceed %" PRIu16,
				    regionNameFmtLen,
				    regionName,
				    scrambleSpecs[region].max
				);
				limit = scrambleSpecs[region].max;
			}

			switch (region) {
			case SCRAMBLE_ROMX:
				options.scrambleROMX = limit;
				break;
			case SCRAMBLE_SRAM:
				options.scrambleSRAM = limit;
				break;
			case SCRAMBLE_WRAMX:
				options.scrambleWRAMX = limit;
				break;
			case SCRAMBLE_UNK: // The error has already been reported, do nothing
				break;
			}
		} else if (region == SCRAMBLE_WRAMX) {
			// Only WRAMX can be implied, since ROMX and SRAM size may vary
			options.scrambleWRAMX = 7;
		} else {
			argErr('S', "Cannot imply limit for region \"%.*s\"", regionNameFmtLen, regionName);
		}

next: // Can't `continue` a `for` loop with this nontrivial iteration logic
		if (spec) {
			assume(*spec == ',' || *spec == '\0');
			if (*spec == ',') {
				spec += 1 + strspn(&spec[1], " \t");
			}
			if (*spec == '\0') {
				break;
			}
		}
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
			// LCOV_EXCL_START
			printUsage();
			exit(0);
			// LCOV_EXCL_STOP
		case 'l':
			if (options.linkerScriptName) {
				warnx("Overriding linker script %s", options.linkerScriptName);
			}
			options.linkerScriptName = musl_optarg;
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

			if (musl_optarg[0] == '\0' || *endptr != '\0' || value > 0xFF) {
				argErr('p', "Argument for 'p' must be a byte (between 0 and 0xFF)");
				value = 0xFF;
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
			options.beVerbose = true;
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
		default:
			// LCOV_EXCL_START
			printUsage();
			exit(1);
			// LCOV_EXCL_STOP
		}
	}

	int curArgIndex = musl_optind;

	// If no input files were specified, the user must have screwed up
	if (curArgIndex == argc) {
		fatalWithUsage("Please specify an input file (pass `-` to read from standard input)");
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
	for (obj_Setup(argc - curArgIndex); curArgIndex < argc; ++curArgIndex) {
		obj_ReadFile(argv[curArgIndex], argc - curArgIndex - 1);
	}

	// apply the linker script's modifications,
	if (options.linkerScriptName) {
		verbosePrint("Reading linker script...\n");

		script_ProcessScript();

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
