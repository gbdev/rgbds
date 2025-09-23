// SPDX-License-Identifier: MIT

#include "fix/main.hpp"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <optional>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostics.hpp"
#include "extern/getopt.hpp"
#include "helpers.hpp"
#include "platform.hpp"
#include "style.hpp"
#include "usage.hpp"
#include "util.hpp"
#include "version.hpp"

#include "fix/fix.hpp"
#include "fix/mbc.hpp"
#include "fix/warning.hpp"

Options options;

// Short options
static char const *optstring = "Ccf:hi:jk:L:l:m:n:Oo:p:r:st:VvW:w";

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
    {"color-only",       no_argument,       nullptr,  'C'},
    {"color-compatible", no_argument,       nullptr,  'c'},
    {"fix-spec",         required_argument, nullptr,  'f'},
    {"help",             no_argument,       nullptr,  'h'},
    {"game-id",          required_argument, nullptr,  'i'},
    {"non-japanese",     no_argument,       nullptr,  'j'},
    {"new-licensee",     required_argument, nullptr,  'k'},
    {"logo",             required_argument, nullptr,  'L'},
    {"old-licensee",     required_argument, nullptr,  'l'},
    {"mbc-type",         required_argument, nullptr,  'm'},
    {"rom-version",      required_argument, nullptr,  'n'},
    {"overwrite",        no_argument,       nullptr,  'O'},
    {"output",           required_argument, nullptr,  'o'},
    {"pad-value",        required_argument, nullptr,  'p'},
    {"ram-size",         required_argument, nullptr,  'r'},
    {"sgb-compatible",   no_argument,       nullptr,  's'},
    {"title",            required_argument, nullptr,  't'},
    {"version",          no_argument,       nullptr,  'V'},
    {"validate",         no_argument,       nullptr,  'v'},
    {"warning",          required_argument, nullptr,  'W'},
    {"color",            required_argument, &longOpt, 'c'},
    {nullptr,            no_argument,       nullptr,  0  },
};

// clang-format off: nested initializers
static Usage usage = {
    .name = "rgbfix",
    .flags = {
        "[-hjOsVvw]", "[-C | -c]", "[-f <fix_spec>]", "[-i <game_id>]", "[-k <licensee>]",
        "[-L <logo_file>]", "[-l <licensee_byte>]", "[-m <mbc_type>]", "[-n <rom_version>]",
        "[-p <pad_value>]", "[-r <ram_size>]", "[-t <title_str>]", "[-W warning]", "<file> ...",
    },
    .options = {
        {
            {"-m", "--mbc-type <value>"},
            {
                "set the MBC type byte to this value; \"-m help\"",
                "or \"-m list\" prints the accepted values",
            },
        },
        {{"-p", "--pad-value <value>"}, {"pad to the next valid size using this value"}},
        {{"-r", "--ram-size <code>"}, {"set the cart RAM size byte to this value"}},
        {{"-o", "--output <path>"}, {"set the output file"}},
        {{"-V", "--version"}, {"print RGBFIX version and exit"}},
        {{"-v", "--validate"}, {"fix the header logo and both checksums (\"-f lhg\")"}},
        {{"-W", "--warning <warning>"}, {"enable or disable warnings"}},
    },
};
// clang-format on

static void parseByte(uint16_t &output, char name) {
	if (musl_optarg[0] == '\0') {
		fatal("Argument to option '-%c' may not be empty", name);
	}

	if (std::optional<uint64_t> value = parseWholeNumber(musl_optarg); !value) {
		fatal("Expected number as argument to option '-%c', got \"%s\"", name, musl_optarg);
	} else if (value > 0xFF) {
		fatal("Argument to option '-%c' is larger than 255: %" PRIu64, name, *value);
	} else {
		output = *value;
	}
}

static uint8_t const nintendoLogo[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

static void initLogo() {
	if (options.logoFilename) {
		FILE *logoFile;
		if (strcmp(options.logoFilename, "-")) {
			logoFile = fopen(options.logoFilename, "rb");
		} else {
			options.logoFilename = "<stdin>";
			(void)setmode(STDIN_FILENO, O_BINARY);
			logoFile = stdin;
		}
		if (!logoFile) {
			fatal("Failed to open \"%s\" for reading: %s", options.logoFilename, strerror(errno));
		}
		Defer closeLogo{[&] { fclose(logoFile); }};

		uint8_t logoBpp[sizeof(options.logo)];
		if (size_t nbRead = fread(logoBpp, 1, sizeof(logoBpp), logoFile);
		    nbRead != sizeof(options.logo) || fgetc(logoFile) != EOF || ferror(logoFile)) {
			fatal("\"%s\" is not %zu bytes", options.logoFilename, sizeof(options.logo));
		}
		auto highs = [&logoBpp](size_t i) {
			return (logoBpp[i * 2] & 0xF0) | ((logoBpp[i * 2 + 1] & 0xF0) >> 4);
		};
		auto lows = [&logoBpp](size_t i) {
			return ((logoBpp[i * 2] & 0x0F) << 4) | (logoBpp[i * 2 + 1] & 0x0F);
		};
		constexpr size_t mid = sizeof(options.logo) / 2;
		for (size_t i = 0; i < mid; i += 4) {
			options.logo[i + 0] = highs(i + 0);
			options.logo[i + 1] = highs(i + 1);
			options.logo[i + 2] = lows(i + 0);
			options.logo[i + 3] = lows(i + 1);
			options.logo[mid + i + 0] = highs(i + 2);
			options.logo[mid + i + 1] = highs(i + 3);
			options.logo[mid + i + 2] = lows(i + 2);
			options.logo[mid + i + 3] = lows(i + 3);
		}
	} else {
		static_assert(sizeof(options.logo) == sizeof(nintendoLogo));
		memcpy(options.logo, nintendoLogo, sizeof(nintendoLogo));
	}

	if (options.fixSpec & TRASH_LOGO) {
		for (uint16_t i = 0; i < sizeof(options.logo); ++i) {
			options.logo[i] = 0xFF ^ options.logo[i];
		}
	}
}

int main(int argc, char *argv[]) {
	char const *outputFilename = nullptr;

	// Parse CLI options
	for (int ch; (ch = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1;) {
		switch (ch) {
		case 'C':
		case 'c':
			options.model = ch == 'c' ? BOTH : CGB;
			if (options.titleLen > 15) {
				options.titleLen = 15;
				assume(options.title != nullptr);
				warning(WARNING_TRUNCATION, "Truncating title \"%s\" to 15 chars", options.title);
			}
			break;

		case 'f':
			options.fixSpec = 0;
			while (*musl_optarg) {
				switch (*musl_optarg) {
#define OVERRIDE_SPEC(cur, bad, curFlag, badFlag) \
	case STR(cur)[0]: \
		if (options.fixSpec & badFlag) { \
			warnx("'" STR(cur) "' overriding '" STR(bad) "' in fix spec"); \
		} \
		options.fixSpec = (options.fixSpec & ~badFlag) | curFlag; \
		break
#define overrideSpecs(fix, fixFlag, trash, trashFlag) \
	OVERRIDE_SPEC(fix, trash, fixFlag, trashFlag); \
	OVERRIDE_SPEC(trash, fix, trashFlag, fixFlag)
					overrideSpecs(l, FIX_LOGO, L, TRASH_LOGO);
					overrideSpecs(h, FIX_HEADER_SUM, H, TRASH_HEADER_SUM);
					overrideSpecs(g, FIX_GLOBAL_SUM, G, TRASH_GLOBAL_SUM);
#undef OVERRIDE_SPEC
#undef overrideSpecs

				default:
					fatal("Invalid character '%c' in fix spec", *musl_optarg);
				}
				++musl_optarg;
			}
			break;

			// LCOV_EXCL_START
		case 'h':
			usage.printAndExit(0);
			// LCOV_EXCL_STOP

		case 'i': {
			options.gameID = musl_optarg;
			size_t len = strlen(options.gameID);
			if (len > 4) {
				len = 4;
				warning(WARNING_TRUNCATION, "Truncating game ID \"%s\" to 4 chars", options.gameID);
			}
			options.gameIDLen = len;
			if (options.titleLen > 11) {
				options.titleLen = 11;
				assume(options.title != nullptr);
				warning(WARNING_TRUNCATION, "Truncating title \"%s\" to 11 chars", options.title);
			}
			break;
		}

		case 'j':
			options.japanese = false;
			break;

		case 'k': {
			options.newLicensee = musl_optarg;
			size_t len = strlen(options.newLicensee);
			if (len > 2) {
				len = 2;
				warning(
				    WARNING_TRUNCATION,
				    "Truncating new licensee \"%s\" to 2 chars",
				    options.newLicensee
				);
			}
			options.newLicenseeLen = len;
			break;
		}

		case 'L':
			options.logoFilename = musl_optarg;
			break;

		case 'l':
			parseByte(options.oldLicensee, 'l');
			break;

		case 'm':
			options.cartridgeType =
			    mbc_ParseName(musl_optarg, options.tpp1Rev[0], options.tpp1Rev[1]);
			if (options.cartridgeType == ROM_RAM || options.cartridgeType == ROM_RAM_BATTERY) {
				warning(
				    WARNING_MBC, "MBC \"%s\" is under-specified and poorly supported", musl_optarg
				);
			}
			break;

		case 'n':
			parseByte(options.romVersion, 'n');
			break;

		case 'O':
			warning(WARNING_OBSOLETE, "'-O' is deprecated; use '-Wno-overwrite' instead");
			warnings.processWarningFlag("no-overwrite");
			break;

		case 'o':
			outputFilename = musl_optarg;
			break;

		case 'p':
			parseByte(options.padValue, 'p');
			break;

		case 'r':
			parseByte(options.ramSize, 'r');
			break;

		case 's':
			options.sgb = true;
			break;

		case 't': {
			options.title = musl_optarg;
			size_t len = strlen(options.title);
			uint8_t maxLen = options.gameID ? 11 : options.model != DMG ? 15 : 16;

			if (len > maxLen) {
				len = maxLen;
				warning(
				    WARNING_TRUNCATION, "Truncating title \"%s\" to %u chars", options.title, maxLen
				);
			}
			options.titleLen = len;
			break;
		}

			// LCOV_EXCL_START
		case 'V':
			printf("rgbfix %s\n", get_package_version_string());
			exit(0);

		case 'v':
			options.fixSpec = FIX_LOGO | FIX_HEADER_SUM | FIX_GLOBAL_SUM;
			break;
			// LCOV_EXCL_STOP

		case 'W':
			warnings.processWarningFlag(musl_optarg);
			break;

		case 'w':
			warnings.state.warningsEnabled = false;
			break;

		case 0: // Long-only options
			if (longOpt == 'c' && !style_Parse(musl_optarg)) {
				fatal("Invalid argument for option '--color'");
			}
			break;

			// LCOV_EXCL_START
		default:
			usage.printAndExit(1);
			// LCOV_EXCL_STOP
		}
	}

	if ((options.cartridgeType & 0xFF00) == TPP1 && !options.japanese) {
		warning(
		    WARNING_MBC, "TPP1 overwrites region flag for its identification code, ignoring '-j'"
		);
	}

	// Check that RAM size is correct for "standard" mappers
	if (options.ramSize != UNSPECIFIED && (options.cartridgeType & 0xFF00) == 0) {
		if (options.cartridgeType == ROM_RAM || options.cartridgeType == ROM_RAM_BATTERY) {
			if (options.ramSize != 1) {
				warning(
				    WARNING_MBC,
				    "MBC \"%s\" should have 2 KiB of RAM (\"-r 1\")",
				    mbc_Name(options.cartridgeType)
				);
			}
		} else if (mbc_HasRAM(options.cartridgeType)) {
			if (!options.ramSize) {
				warning(
				    WARNING_MBC,
				    "MBC \"%s\" has RAM, but RAM size was set to 0",
				    mbc_Name(options.cartridgeType)
				);
			} else if (options.ramSize == 1) {
				warning(
				    WARNING_MBC,
				    "RAM size 1 (2 KiB) was specified for MBC \"%s\"",
				    mbc_Name(options.cartridgeType)
				);
			}
		} else if (options.ramSize) {
			warning(
			    WARNING_MBC,
			    "MBC \"%s\" has no RAM, but RAM size was set to %u",
			    mbc_Name(options.cartridgeType),
			    options.ramSize
			);
		}
	}

	if (options.sgb && options.oldLicensee != UNSPECIFIED && options.oldLicensee != 0x33) {
		warning(
		    WARNING_SGB,
		    "SGB compatibility enabled, but old licensee is 0x%02x, not 0x33",
		    options.oldLicensee
		);
	}

	initLogo();

	argv += musl_optind;
	if (!*argv) {
		usage.printAndExit("No input file specified (pass \"-\" to read from standard input)");
	}

	if (outputFilename && argc != musl_optind + 1) {
		usage.printAndExit("If '-o' is set then only a single input file may be specified");
	}

	bool failed = warnings.nbErrors > 0;
	do {
		failed |= fix_ProcessFile(*argv, outputFilename);
	} while (*++argv);

	return failed;
}
