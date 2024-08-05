/* SPDX-License-Identifier: MIT */

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "extern/getopt.hpp"
#include "helpers.hpp"
#include "platform.hpp"
#include "version.hpp"

#define UNSPECIFIED 0x200 // Should not be in byte range

#define BANK_SIZE 0x4000

// Short options
static char const *optstring = "Ccf:i:jk:L:l:m:n:Op:r:st:Vv";

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
static option const longopts[] = {
    {"color-only",       no_argument,       nullptr, 'C'},
    {"color-compatible", no_argument,       nullptr, 'c'},
    {"fix-spec",         required_argument, nullptr, 'f'},
    {"game-id",          required_argument, nullptr, 'i'},
    {"non-japanese",     no_argument,       nullptr, 'j'},
    {"new-licensee",     required_argument, nullptr, 'k'},
    {"logo",             required_argument, nullptr, 'L'},
    {"old-licensee",     required_argument, nullptr, 'l'},
    {"mbc-type",         required_argument, nullptr, 'm'},
    {"rom-version",      required_argument, nullptr, 'n'},
    {"overwrite",        no_argument,       nullptr, 'O'},
    {"pad-value",        required_argument, nullptr, 'p'},
    {"ram-size",         required_argument, nullptr, 'r'},
    {"sgb-compatible",   no_argument,       nullptr, 's'},
    {"title",            required_argument, nullptr, 't'},
    {"version",          no_argument,       nullptr, 'V'},
    {"validate",         no_argument,       nullptr, 'v'},
    {nullptr,            no_argument,       nullptr, 0  }
};

static void printUsage() {
	fputs(
	    "Usage: rgbfix [-jOsVv] [-C | -c] [-f <fix_spec>] [-i <game_id>] [-k <licensee>]\n"
	    "              [-L <logo_file>] [-l <licensee_byte>] [-m <mbc_type>]\n"
	    "              [-n <rom_version>] [-p <pad_value>] [-r <ram_size>] [-t <title_str>]\n"
	    "              <file> ...\n"
	    "Useful options:\n"
	    "    -m, --mbc-type <value>      set the MBC type byte to this value; refer\n"
	    "                                  to the man page for a list of values\n"
	    "    -p, --pad-value <value>     pad to the next valid size using this value\n"
	    "    -r, --ram-size <code>       set the cart RAM size byte to this value\n"
	    "    -V, --version               print RGBFIX version and exit\n"
	    "    -v, --validate              fix the header logo and both checksums (-f lhg)\n"
	    "\n"
	    "For help, use `man rgbfix' or go to https://rgbds.gbdev.io/docs/\n",
	    stderr
	);
}

static uint8_t nbErrors;

[[gnu::format(printf, 1, 2)]] static void report(char const *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (nbErrors != UINT8_MAX)
		nbErrors++;
}

enum MbcType {
	ROM = 0x00,
	ROM_RAM = 0x08,
	ROM_RAM_BATTERY = 0x09,

	MBC1 = 0x01,
	MBC1_RAM = 0x02,
	MBC1_RAM_BATTERY = 0x03,

	MBC2 = 0x05,
	MBC2_BATTERY = 0x06,

	MMM01 = 0x0B,
	MMM01_RAM = 0x0C,
	MMM01_RAM_BATTERY = 0x0D,

	MBC3 = 0x11,
	MBC3_TIMER_BATTERY = 0x0F,
	MBC3_TIMER_RAM_BATTERY = 0x10,
	MBC3_RAM = 0x12,
	MBC3_RAM_BATTERY = 0x13,

	MBC5 = 0x19,
	MBC5_RAM = 0x1A,
	MBC5_RAM_BATTERY = 0x1B,
	MBC5_RUMBLE = 0x1C,
	MBC5_RUMBLE_RAM = 0x1D,
	MBC5_RUMBLE_RAM_BATTERY = 0x1E,

	MBC6 = 0x20,

	MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,

	POCKET_CAMERA = 0xFC,

	BANDAI_TAMA5 = 0xFD,

	HUC3 = 0xFE,

	HUC1_RAM_BATTERY = 0xFF,

	// "Extended" values (still valid, but not directly actionable)

	// A high byte of 0x01 means TPP1, the low byte is the requested features
	// This does not include SRAM, which is instead implied by a non-zero SRAM size
	// Note: Multiple rumble speeds imply rumble
	TPP1 = 0x100,
	TPP1_RUMBLE = 0x101,
	TPP1_MULTIRUMBLE = 0x102, // Should not be possible
	TPP1_MULTIRUMBLE_RUMBLE = 0x103,
	TPP1_TIMER = 0x104,
	TPP1_TIMER_RUMBLE = 0x105,
	TPP1_TIMER_MULTIRUMBLE = 0x106, // Should not be possible
	TPP1_TIMER_MULTIRUMBLE_RUMBLE = 0x107,
	TPP1_BATTERY = 0x108,
	TPP1_BATTERY_RUMBLE = 0x109,
	TPP1_BATTERY_MULTIRUMBLE = 0x10A, // Should not be possible
	TPP1_BATTERY_MULTIRUMBLE_RUMBLE = 0x10B,
	TPP1_BATTERY_TIMER = 0x10C,
	TPP1_BATTERY_TIMER_RUMBLE = 0x10D,
	TPP1_BATTERY_TIMER_MULTIRUMBLE = 0x10E, // Should not be possible
	TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE = 0x10F,

	// Error values
	MBC_NONE = UNSPECIFIED, // No MBC specified, do not act on it
	MBC_BAD,                // Specified MBC does not exist / syntax error
	MBC_WRONG_FEATURES,     // MBC incompatible with specified features
	MBC_BAD_RANGE,          // MBC number out of range
};

static void printAcceptedMBCNames() {
	fputs("\tROM ($00) [aka ROM_ONLY]\n", stderr);
	fputs("\tMBC1 ($01), MBC1+RAM ($02), MBC1+RAM+BATTERY ($03)\n", stderr);
	fputs("\tMBC2 ($05), MBC2+BATTERY ($06)\n", stderr);
	fputs("\tROM+RAM ($08) [deprecated], ROM+RAM+BATTERY ($09) [deprecated]\n", stderr);
	fputs("\tMMM01 ($0B), MMM01+RAM ($0C), MMM01+RAM+BATTERY ($0D)\n", stderr);
	fputs("\tMBC3+TIMER+BATTERY ($0F), MBC3+TIMER+RAM+BATTERY ($10)\n", stderr);
	fputs("\tMBC3 ($11), MBC3+RAM ($12), MBC3+RAM+BATTERY ($13)\n", stderr);
	fputs("\tMBC5 ($19), MBC5+RAM ($1A), MBC5+RAM+BATTERY ($1B)\n", stderr);
	fputs("\tMBC5+RUMBLE ($1C), MBC5+RUMBLE+RAM ($1D), MBC5+RUMBLE+RAM+BATTERY ($1E)\n", stderr);
	fputs("\tMBC6 ($20)\n", stderr);
	fputs("\tMBC7+SENSOR+RUMBLE+RAM+BATTERY ($22)\n", stderr);
	fputs("\tPOCKET_CAMERA ($FC)\n", stderr);
	fputs("\tBANDAI_TAMA5 ($FD)\n", stderr);
	fputs("\tHUC3 ($FE)\n", stderr);
	fputs("\tHUC1+RAM+BATTERY ($FF)\n", stderr);

	fputs("\n\tTPP1_1.0, TPP1_1.0+RUMBLE, TPP1_1.0+MULTIRUMBLE, TPP1_1.0+TIMER,\n", stderr);
	fputs("\tTPP1_1.0+TIMER+RUMBLE, TPP1_1.0+TIMER+MULTIRUMBLE, TPP1_1.0+BATTERY,\n", stderr);
	fputs("\tTPP1_1.0+BATTERY+RUMBLE, TPP1_1.0+BATTERY+MULTIRUMBLE,\n", stderr);
	fputs("\tTPP1_1.0+BATTERY+TIMER, TPP1_1.0+BATTERY+TIMER+RUMBLE,\n", stderr);
	fputs("\tTPP1_1.0+BATTERY+TIMER+MULTIRUMBLE\n", stderr);
}

static uint8_t tpp1Rev[2];

/*
 * @return False on failure
 */
static bool readMBCSlice(char const *&name, char const *expected) {
	while (*expected) {
		char c = *name++;

		if (c == '\0') // Name too short
			return false;

		if (c >= 'a' && c <= 'z') // Perform the comparison case-insensitive
			c = c - 'a' + 'A';
		else if (c == '_') // Treat underscores as spaces
			c = ' ';

		if (c != *expected++)
			return false;
	}
	return true;
}

static MbcType parseMBC(char const *name) {
	if (!strcasecmp(name, "help")) {
		fputs("Accepted MBC names:\n", stderr);
		printAcceptedMBCNames();
		exit(0);
	}

	if ((name[0] >= '0' && name[0] <= '9') || name[0] == '$') {
		int base = 0;

		if (name[0] == '$') {
			name++;
			base = 16;
		}
		// Parse number, and return it as-is (unless it's too large)
		char *endptr;
		unsigned long mbc = strtoul(name, &endptr, base);

		if (*endptr)
			return MBC_BAD;
		if (mbc > 0xFF)
			return MBC_BAD_RANGE;
		return (MbcType)mbc;

	} else {
		// Begin by reading the MBC type:
		uint16_t mbc;
		char const *ptr = name;

		// Trim off leading whitespace
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;

#define tryReadSlice(expected) \
	do { \
		if (!readMBCSlice(ptr, expected)) \
			return MBC_BAD; \
	} while (0)

		switch (*ptr++) {
		case 'R': // ROM / ROM_ONLY
		case 'r':
			tryReadSlice("OM");
			// Handle optional " ONLY"
			while (*ptr == ' ' || *ptr == '\t' || *ptr == '_')
				ptr++;
			if (*ptr == 'O' || *ptr == 'o') {
				ptr++;
				tryReadSlice("NLY");
			}
			mbc = ROM;
			break;

		case 'M': // MBC{1, 2, 3, 5, 6, 7} / MMM01
		case 'm':
			switch (*ptr++) {
			case 'B':
			case 'b':
				switch (*ptr++) {
				case 'C':
				case 'c':
					break;
				default:
					return MBC_BAD;
				}
				switch (*ptr++) {
				case '1':
					mbc = MBC1;
					break;
				case '2':
					mbc = MBC2;
					break;
				case '3':
					mbc = MBC3;
					break;
				case '5':
					mbc = MBC5;
					break;
				case '6':
					mbc = MBC6;
					break;
				case '7':
					mbc = MBC7_SENSOR_RUMBLE_RAM_BATTERY;
					break;
				default:
					return MBC_BAD;
				}
				break;
			case 'M':
			case 'm':
				tryReadSlice("M01");
				mbc = MMM01;
				break;
			default:
				return MBC_BAD;
			}
			break;

		case 'P': // POCKET_CAMERA
		case 'p':
			tryReadSlice("OCKET CAMERA");
			mbc = POCKET_CAMERA;
			break;

		case 'B': // BANDAI_TAMA5
		case 'b':
			tryReadSlice("ANDAI TAMA5");
			mbc = BANDAI_TAMA5;
			break;

		case 'T': // TAMA5 / TPP1
		case 't':
			switch (*ptr++) {
			case 'A':
				tryReadSlice("MA5");
				mbc = BANDAI_TAMA5;
				break;
			case 'P': {
				tryReadSlice("P1");
				// Parse version
				while (*ptr == ' ' || *ptr == '_')
					ptr++;
				// Major
				char *endptr;
				unsigned long val = strtoul(ptr, &endptr, 10);

				if (endptr == ptr) {
					report("error: Failed to parse TPP1 major revision number\n");
					return MBC_BAD;
				}
				ptr = endptr;
				if (val != 1) {
					report("error: RGBFIX only supports TPP1 versions 1.0\n");
					return MBC_BAD;
				}
				tpp1Rev[0] = val;
				tryReadSlice(".");
				// Minor
				val = strtoul(ptr, &endptr, 10);
				if (endptr == ptr) {
					report("error: Failed to parse TPP1 minor revision number\n");
					return MBC_BAD;
				}
				ptr = endptr;
				if (val > 0xFF) {
					report("error: TPP1 minor revision number must be 8-bit\n");
					return MBC_BAD;
				}
				tpp1Rev[1] = val;
				mbc = TPP1;
				break;
			}
			default:
				return MBC_BAD;
			}
			break;

		case 'H': // HuC{1, 3}
		case 'h':
			tryReadSlice("UC");
			switch (*ptr++) {
			case '1':
				mbc = HUC1_RAM_BATTERY;
				break;
			case '3':
				mbc = HUC3;
				break;
			default:
				return MBC_BAD;
			}
			break;

		default:
			return MBC_BAD;
		}

		// Read "additional features"
		uint8_t features = 0;
#define RAM         (1 << 7)
#define BATTERY     (1 << 6)
#define TIMER       (1 << 5)
#define RUMBLE      (1 << 4)
#define SENSOR      (1 << 3)
#define MULTIRUMBLE (1 << 2)

		for (;;) {
			// Trim off trailing whitespace
			while (*ptr == ' ' || *ptr == '\t' || *ptr == '_')
				ptr++;

			// If done, start processing "features"
			if (!*ptr)
				break;
			// We expect a '+' at this point
			if (*ptr++ != '+')
				return MBC_BAD;
			// Trim off leading whitespace
			while (*ptr == ' ' || *ptr == '\t' || *ptr == '_')
				ptr++;

			switch (*ptr++) {
			case 'B': // BATTERY
			case 'b':
				tryReadSlice("ATTERY");
				features |= BATTERY;
				break;

			case 'M':
			case 'm':
				tryReadSlice("ULTIRUMBLE");
				features |= MULTIRUMBLE;
				break;

			case 'R': // RAM or RUMBLE
			case 'r':
				switch (*ptr++) {
				case 'U':
				case 'u':
					tryReadSlice("MBLE");
					features |= RUMBLE;
					break;
				case 'A':
				case 'a':
					if (*ptr != 'M' && *ptr != 'm')
						return MBC_BAD;
					ptr++;
					features |= RAM;
					break;
				default:
					return MBC_BAD;
				}
				break;

			case 'S': // SENSOR
			case 's':
				tryReadSlice("ENSOR");
				features |= SENSOR;
				break;

			case 'T': // TIMER
			case 't':
				tryReadSlice("IMER");
				features |= TIMER;
				break;

			default:
				return MBC_BAD;
			}
		}
#undef tryReadSlice

		switch (mbc) {
		case ROM:
			if (!features)
				break;
			mbc = ROM_RAM - 1;
			static_assert(ROM_RAM + 1 == ROM_RAM_BATTERY, "Enum sanity check failed!");
			static_assert(MBC1 + 1 == MBC1_RAM, "Enum sanity check failed!");
			static_assert(MBC1 + 2 == MBC1_RAM_BATTERY, "Enum sanity check failed!");
			static_assert(MMM01 + 1 == MMM01_RAM, "Enum sanity check failed!");
			static_assert(MMM01 + 2 == MMM01_RAM_BATTERY, "Enum sanity check failed!");
			[[fallthrough]];
		case MBC1:
		case MMM01:
			if (features == RAM)
				mbc++;
			else if (features == (RAM | BATTERY))
				mbc += 2;
			else if (features)
				return MBC_WRONG_FEATURES;
			break;

		case MBC2:
			if (features == BATTERY)
				mbc = MBC2_BATTERY;
			else if (features)
				return MBC_WRONG_FEATURES;
			break;

		case MBC3:
			// Handle timer, which also requires battery
			if (features & TIMER) {
				if (!(features & BATTERY))
					fprintf(stderr, "warning: MBC3+TIMER implies BATTERY\n");
				features &= ~(TIMER | BATTERY); // Reset those bits
				mbc = MBC3_TIMER_BATTERY;
				// RAM is handled below
			}
			static_assert(MBC3 + 1 == MBC3_RAM, "Enum sanity check failed!");
			static_assert(MBC3 + 2 == MBC3_RAM_BATTERY, "Enum sanity check failed!");
			static_assert(
			    MBC3_TIMER_BATTERY + 1 == MBC3_TIMER_RAM_BATTERY, "Enum sanity check failed!"
			);
			if (features == RAM)
				mbc++;
			else if (features == (RAM | BATTERY))
				mbc += 2;
			else if (features)
				return MBC_WRONG_FEATURES;
			break;

		case MBC5:
			if (features & RUMBLE) {
				features &= ~RUMBLE;
				mbc = MBC5_RUMBLE;
			}
			static_assert(MBC5 + 1 == MBC5_RAM, "Enum sanity check failed!");
			static_assert(MBC5 + 2 == MBC5_RAM_BATTERY, "Enum sanity check failed!");
			static_assert(MBC5_RUMBLE + 1 == MBC5_RUMBLE_RAM, "Enum sanity check failed!");
			static_assert(MBC5_RUMBLE + 2 == MBC5_RUMBLE_RAM_BATTERY, "Enum sanity check failed!");
			if (features == RAM)
				mbc++;
			else if (features == (RAM | BATTERY))
				mbc += 2;
			else if (features)
				return MBC_WRONG_FEATURES;
			break;

		case MBC6:
		case POCKET_CAMERA:
		case BANDAI_TAMA5:
		case HUC3:
			// No extra features accepted
			if (features)
				return MBC_WRONG_FEATURES;
			break;

		case MBC7_SENSOR_RUMBLE_RAM_BATTERY:
			if (features != (SENSOR | RUMBLE | RAM | BATTERY))
				return MBC_WRONG_FEATURES;
			break;

		case HUC1_RAM_BATTERY:
			if (features != (RAM | BATTERY)) // HuC1 expects RAM+BATTERY
				return MBC_WRONG_FEATURES;
			break;

		case TPP1:
			if (features & RAM)
				fprintf(
				    stderr, "warning: TPP1 requests RAM implicitly if given a non-zero RAM size"
				);
			if (features & BATTERY)
				mbc |= 0x08;
			if (features & TIMER)
				mbc |= 0x04;
			if (features & MULTIRUMBLE)
				mbc |= 0x03; // Also set the rumble flag
			if (features & RUMBLE)
				mbc |= 0x01;
			if (features & SENSOR)
				return MBC_WRONG_FEATURES;
			break;
		}

		// Trim off trailing whitespace
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;

		// If there is still something past the whitespace, error out
		if (*ptr)
			return MBC_BAD;

		return (MbcType)mbc;
	}
}

static char const *mbcName(MbcType type) {
	switch (type) {
	case ROM:
		return "ROM";
	case ROM_RAM:
		return "ROM+RAM";
	case ROM_RAM_BATTERY:
		return "ROM+RAM+BATTERY";
	case MBC1:
		return "MBC1";
	case MBC1_RAM:
		return "MBC1+RAM";
	case MBC1_RAM_BATTERY:
		return "MBC1+RAM+BATTERY";
	case MBC2:
		return "MBC2";
	case MBC2_BATTERY:
		return "MBC2+BATTERY";
	case MMM01:
		return "MMM01";
	case MMM01_RAM:
		return "MMM01+RAM";
	case MMM01_RAM_BATTERY:
		return "MMM01+RAM+BATTERY";
	case MBC3:
		return "MBC3";
	case MBC3_TIMER_BATTERY:
		return "MBC3+TIMER+BATTERY";
	case MBC3_TIMER_RAM_BATTERY:
		return "MBC3+TIMER+RAM+BATTERY";
	case MBC3_RAM:
		return "MBC3+RAM";
	case MBC3_RAM_BATTERY:
		return "MBC3+RAM+BATTERY";
	case MBC5:
		return "MBC5";
	case MBC5_RAM:
		return "MBC5+RAM";
	case MBC5_RAM_BATTERY:
		return "MBC5+RAM+BATTERY";
	case MBC5_RUMBLE:
		return "MBC5+RUMBLE";
	case MBC5_RUMBLE_RAM:
		return "MBC5+RUMBLE+RAM";
	case MBC5_RUMBLE_RAM_BATTERY:
		return "MBC5+RUMBLE+RAM+BATTERY";
	case MBC6:
		return "MBC6";
	case MBC7_SENSOR_RUMBLE_RAM_BATTERY:
		return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
	case POCKET_CAMERA:
		return "POCKET CAMERA";
	case BANDAI_TAMA5:
		return "BANDAI TAMA5";
	case HUC3:
		return "HUC3";
	case HUC1_RAM_BATTERY:
		return "HUC1+RAM+BATTERY";
	case TPP1:
		return "TPP1";
	case TPP1_RUMBLE:
		return "TPP1+RUMBLE";
	case TPP1_MULTIRUMBLE:
	case TPP1_MULTIRUMBLE_RUMBLE:
		return "TPP1+MULTIRUMBLE";
	case TPP1_TIMER:
		return "TPP1+TIMER";
	case TPP1_TIMER_RUMBLE:
		return "TPP1+TIMER+RUMBLE";
	case TPP1_TIMER_MULTIRUMBLE:
	case TPP1_TIMER_MULTIRUMBLE_RUMBLE:
		return "TPP1+TIMER+MULTIRUMBLE";
	case TPP1_BATTERY:
		return "TPP1+BATTERY";
	case TPP1_BATTERY_RUMBLE:
		return "TPP1+BATTERY+RUMBLE";
	case TPP1_BATTERY_MULTIRUMBLE:
	case TPP1_BATTERY_MULTIRUMBLE_RUMBLE:
		return "TPP1+BATTERY+MULTIRUMBLE";
	case TPP1_BATTERY_TIMER:
		return "TPP1+BATTERY+TIMER";
	case TPP1_BATTERY_TIMER_RUMBLE:
		return "TPP1+BATTERY+TIMER+RUMBLE";
	case TPP1_BATTERY_TIMER_MULTIRUMBLE:
	case TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE:
		return "TPP1+BATTERY+TIMER+MULTIRUMBLE";

	// Error values
	case MBC_NONE:
	case MBC_BAD:
	case MBC_WRONG_FEATURES:
	case MBC_BAD_RANGE:
		unreachable_();
	}

	unreachable_();
}

static bool hasRAM(MbcType type) {
	switch (type) {
	case ROM:
	case MBC1:
	case MBC2: // Technically has RAM, but not marked as such
	case MBC2_BATTERY:
	case MMM01:
	case MBC3:
	case MBC3_TIMER_BATTERY:
	case MBC5:
	case MBC5_RUMBLE:
	case MBC6:         // TODO: not sure
	case BANDAI_TAMA5: // TODO: not sure
	case MBC_NONE:
	case MBC_BAD:
	case MBC_WRONG_FEATURES:
	case MBC_BAD_RANGE:
		return false;

	case ROM_RAM:
	case ROM_RAM_BATTERY:
	case MBC1_RAM:
	case MBC1_RAM_BATTERY:
	case MMM01_RAM:
	case MMM01_RAM_BATTERY:
	case MBC3_TIMER_RAM_BATTERY:
	case MBC3_RAM:
	case MBC3_RAM_BATTERY:
	case MBC5_RAM:
	case MBC5_RAM_BATTERY:
	case MBC5_RUMBLE_RAM:
	case MBC5_RUMBLE_RAM_BATTERY:
	case MBC7_SENSOR_RUMBLE_RAM_BATTERY:
	case POCKET_CAMERA:
	case HUC3:
	case HUC1_RAM_BATTERY:
		return true;

	// TPP1 may or may not have RAM, don't call this function for it
	case TPP1:
	case TPP1_RUMBLE:
	case TPP1_MULTIRUMBLE:
	case TPP1_MULTIRUMBLE_RUMBLE:
	case TPP1_TIMER:
	case TPP1_TIMER_RUMBLE:
	case TPP1_TIMER_MULTIRUMBLE:
	case TPP1_TIMER_MULTIRUMBLE_RUMBLE:
	case TPP1_BATTERY:
	case TPP1_BATTERY_RUMBLE:
	case TPP1_BATTERY_MULTIRUMBLE:
	case TPP1_BATTERY_MULTIRUMBLE_RUMBLE:
	case TPP1_BATTERY_TIMER:
	case TPP1_BATTERY_TIMER_RUMBLE:
	case TPP1_BATTERY_TIMER_MULTIRUMBLE:
	case TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE:
		break;
	}

	unreachable_();
}

static uint8_t const nintendoLogo[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

static uint8_t fixSpec = 0;
#define FIX_LOGO         (1 << 7)
#define TRASH_LOGO       (1 << 6)
#define FIX_HEADER_SUM   (1 << 5)
#define TRASH_HEADER_SUM (1 << 4)
#define FIX_GLOBAL_SUM   (1 << 3)
#define TRASH_GLOBAL_SUM (1 << 2)

static enum { DMG, BOTH, CGB } model = DMG; // If DMG, byte is left alone
static char const *gameID = nullptr;
static uint8_t gameIDLen;
static bool japanese = true;
static char const *logoFilename = nullptr;
static uint8_t logo[sizeof(nintendoLogo)] = {};
static char const *newLicensee = nullptr;
static uint8_t newLicenseeLen;
static uint16_t oldLicensee = UNSPECIFIED;
static MbcType cartridgeType = MBC_NONE;
static uint16_t romVersion = UNSPECIFIED;
static bool overwriteRom = false; // If false, warn when overwriting non-zero non-identical bytes
static uint16_t padValue = UNSPECIFIED;
static uint16_t ramSize = UNSPECIFIED;
static bool sgb = false; // If false, SGB flags are left alone
static char const *title = nullptr;
static uint8_t titleLen;

static uint8_t maxTitleLen() {
	return gameID ? 11 : model != DMG ? 15 : 16;
}

static ssize_t readBytes(int fd, uint8_t *buf, size_t len) {
	// POSIX specifies that lengths greater than SSIZE_MAX yield implementation-defined results
	assume(len <= SSIZE_MAX);

	ssize_t total = 0;

	while (len) {
		ssize_t ret = read(fd, buf, len);

		if (ret == -1 && errno != EINTR) // Return errors, unless we only were interrupted
			return -1;
		// EOF reached
		if (ret == 0)
			return total;
		// If anything was read, accumulate it, and continue
		if (ret != -1) {
			total += ret;
			len -= ret;
			buf += ret;
		}
	}

	return total;
}

static ssize_t writeBytes(int fd, uint8_t *buf, size_t len) {
	// POSIX specifies that lengths greater than SSIZE_MAX yield implementation-defined results
	assume(len <= SSIZE_MAX);

	ssize_t total = 0;

	while (len) {
		ssize_t ret = write(fd, buf, len);

		if (ret == -1 && errno != EINTR) // Return errors, unless we only were interrupted
			return -1;
		// EOF reached
		if (ret == 0)
			return total;
		// If anything was read, accumulate it, and continue
		if (ret != -1) {
			total += ret;
			len -= ret;
		}
	}

	return total;
}

/*
 * @param rom0 A pointer to rom0
 * @param addr What address to check
 * @param fixedByte The fixed byte at the address
 * @param areaName Name to be displayed in the warning message
 */
static void overwriteByte(uint8_t *rom0, uint16_t addr, uint8_t fixedByte, char const *areaName) {
	uint8_t origByte = rom0[addr];

	if (!overwriteRom && origByte != 0 && origByte != fixedByte)
		fprintf(stderr, "warning: Overwrote a non-zero byte in the %s\n", areaName);

	rom0[addr] = fixedByte;
}

/*
 * @param rom0 A pointer to rom0
 * @param startAddr What address to begin checking from
 * @param fixed The fixed bytes at the address
 * @param size How many bytes to check
 * @param areaName Name to be displayed in the warning message
 */
static void overwriteBytes(
    uint8_t *rom0, uint16_t startAddr, uint8_t const *fixed, uint8_t size, char const *areaName
) {
	if (!overwriteRom) {
		for (uint8_t i = 0; i < size; i++) {
			uint8_t origByte = rom0[i + startAddr];

			if (origByte != 0 && origByte != fixed[i]) {
				fprintf(stderr, "warning: Overwrote a non-zero byte in the %s\n", areaName);
				break;
			}
		}
	}

	memcpy(&rom0[startAddr], fixed, size);
}

/*
 * @param input File descriptor to be used for reading
 * @param output File descriptor to be used for writing, may be equal to `input`
 * @param name The file's name, to be displayed for error output
 * @param fileSize The file's size if known, 0 if not.
 */
static void processFile(int input, int output, char const *name, off_t fileSize) {
	// Both of these should be true for seekable files, and neither otherwise
	if (input == output)
		assume(fileSize != 0);
	else
		assume(fileSize == 0);

	uint8_t rom0[BANK_SIZE];
	ssize_t rom0Len = readBytes(input, rom0, sizeof(rom0));
	// Also used as how many bytes to write back when fixing in-place
	ssize_t headerSize = (cartridgeType & 0xFF00) == TPP1 ? 0x154 : 0x150;

	if (rom0Len == -1) {
		report("FATAL: Failed to read \"%s\"'s header: %s\n", name, strerror(errno));
		return;
	} else if (rom0Len < headerSize) {
		report(
		    "FATAL: \"%s\" too short, expected at least %jd ($%jx) bytes, got only %jd\n",
		    name,
		    (intmax_t)headerSize,
		    (intmax_t)headerSize,
		    (intmax_t)rom0Len
		);
		return;
	}
	// Accept partial reads if the file contains at least the header

	if (fixSpec & (FIX_LOGO | TRASH_LOGO))
		overwriteBytes(rom0, 0x0104, logo, sizeof(logo), logoFilename ? "logo" : "Nintendo logo");

	if (title)
		overwriteBytes(rom0, 0x134, (uint8_t const *)title, titleLen, "title");

	if (gameID)
		overwriteBytes(rom0, 0x13F, (uint8_t const *)gameID, gameIDLen, "manufacturer code");

	if (model != DMG)
		overwriteByte(rom0, 0x143, model == BOTH ? 0x80 : 0xC0, "CGB flag");

	if (newLicensee)
		overwriteBytes(
		    rom0, 0x144, (uint8_t const *)newLicensee, newLicenseeLen, "new licensee code"
		);

	if (sgb)
		overwriteByte(rom0, 0x146, 0x03, "SGB flag");

	// If a valid MBC was specified...
	if (cartridgeType < MBC_NONE) {
		uint8_t byte = cartridgeType;

		if ((cartridgeType & 0xFF00) == TPP1) {
			// Cartridge type isn't directly actionable, translate it
			byte = 0xBC;
			// The other TPP1 identification bytes will be written below
		}
		overwriteByte(rom0, 0x147, byte, "cartridge type");
	}

	// ROM size will be written last, after evaluating the file's size

	if ((cartridgeType & 0xFF00) == TPP1) {
		uint8_t const tpp1Code[2] = {0xC1, 0x65};

		overwriteBytes(rom0, 0x149, tpp1Code, sizeof(tpp1Code), "TPP1 identification code");

		overwriteBytes(rom0, 0x150, tpp1Rev, sizeof(tpp1Rev), "TPP1 revision number");

		if (ramSize != UNSPECIFIED)
			overwriteByte(rom0, 0x152, ramSize, "RAM size");

		overwriteByte(rom0, 0x153, cartridgeType & 0xFF, "TPP1 feature flags");
	} else {
		// Regular mappers

		if (ramSize != UNSPECIFIED)
			overwriteByte(rom0, 0x149, ramSize, "RAM size");

		if (!japanese)
			overwriteByte(rom0, 0x14A, 0x01, "destination code");
	}

	if (oldLicensee != UNSPECIFIED)
		overwriteByte(rom0, 0x14B, oldLicensee, "old licensee code");
	else if (sgb && rom0[0x14B] != 0x33)
		fprintf(
		    stderr,
		    "warning: SGB compatibility enabled, but old licensee was 0x%02x, not 0x33\n",
		    rom0[0x14B]
		);

	if (romVersion != UNSPECIFIED)
		overwriteByte(rom0, 0x14C, romVersion, "mask ROM version number");

	// Remain to be handled the ROM size, and header checksum.
	// The latter depends on the former, and so will be handled after it.
	// The former requires knowledge of the file's total size, so read that first.

	uint16_t globalSum = 0;

	// To keep file sizes fairly reasonable, we'll cap the amount of banks at 65536.
	// Official mappers only go up to 512 banks, but at least the TPP1 spec allows up to
	// 65536 banks = 1 GiB.
	// This should be reasonable for the time being, and may be extended later.
	std::vector<uint8_t> romx; // Buffer of ROMX bank data
	uint32_t nbBanks = 1;      // Number of banks *targeted*, including ROM0
	size_t totalRomxLen = 0;   // *Actual* size of ROMX data
	uint8_t bank[BANK_SIZE];   // Temp buffer used to store a whole bank's worth of data

	// Handle ROMX
	if (input == output) {
		if (fileSize >= 0x10000 * BANK_SIZE) {
			report("FATAL: \"%s\" has more than 65536 banks\n", name);
			return;
		}
		// This should be guaranteed from the size cap...
		static_assert(0x10000 * BANK_SIZE <= SSIZE_MAX, "Max input file size too large for OS");
		// Compute number of banks and ROMX len from file size
		nbBanks = (fileSize + (BANK_SIZE - 1)) / BANK_SIZE;
		//      = ceil(totalRomxLen / BANK_SIZE)
		totalRomxLen = fileSize >= BANK_SIZE ? fileSize - BANK_SIZE : 0;
	} else if (rom0Len == BANK_SIZE) {
		// Copy ROMX when reading a pipe, and we're not at EOF yet
		for (;;) {
			romx.resize(nbBanks * BANK_SIZE);
			ssize_t bankLen = readBytes(input, &romx[(nbBanks - 1) * BANK_SIZE], BANK_SIZE);

			// Update bank count, ONLY IF at least one byte was read
			if (bankLen) {
				// We're gonna read another bank, check that it won't be too much
				static_assert(
				    0x10000 * BANK_SIZE <= SSIZE_MAX, "Max input file size too large for OS"
				);
				if (nbBanks == 0x10000) {
					report("FATAL: \"%s\" has more than 65536 banks\n", name);
					return;
				}
				nbBanks++;

				// Update global checksum, too
				for (uint16_t i = 0; i < bankLen; i++)
					globalSum += romx[totalRomxLen + i];
				totalRomxLen += bankLen;
			}
			// Stop when an incomplete bank has been read
			if (bankLen != BANK_SIZE)
				break;
		}
	}

	// Handle setting the ROM size if padding was requested
	// Pad to the next valid power of 2. This is because padding is required by flashers, which
	// flash to ROM chips, whose size is always a power of 2... so there'd be no point in
	// padding to something else.
	// Additionally, a ROM must be at least 32k, so we guarantee a whole amount of banks...
	if (padValue != UNSPECIFIED) {
		// We want at least 2 banks
		if (nbBanks == 1) {
			if (rom0Len != sizeof(rom0)) {
				memset(&rom0[rom0Len], padValue, sizeof(rom0) - rom0Len);
				// The global checksum hasn't taken ROM0 into consideration yet!
				// ROM0 was padded, so treat it as entirely written: update its size
				// Update how many bytes were read in total, too
				rom0Len = sizeof(rom0);
			}
			nbBanks = 2;
		} else {
			assume(rom0Len == sizeof(rom0));
		}
		assume(nbBanks >= 2);
		// Alter number of banks to reflect required value
		// x&(x-1) is zero iff x is a power of 2, or 0; we know for sure it's non-zero,
		// so this is true (non-zero) when we don't have a power of 2
		if (nbBanks & (nbBanks - 1))
			nbBanks = 1 << (CHAR_BIT * sizeof(nbBanks) - clz(nbBanks));
		// Write final ROM size
		rom0[0x148] = ctz(nbBanks / 2);
		// Alter global checksum based on how many bytes will be added (not counting ROM0)
		globalSum += padValue * ((nbBanks - 1) * BANK_SIZE - totalRomxLen);
	}

	// Handle the header checksum after the ROM size has been written
	if (fixSpec & (FIX_HEADER_SUM | TRASH_HEADER_SUM)) {
		uint8_t sum = 0;

		for (uint16_t i = 0x134; i < 0x14D; i++)
			sum -= rom0[i] + 1;

		overwriteByte(rom0, 0x14D, fixSpec & TRASH_HEADER_SUM ? ~sum : sum, "header checksum");
	}

	if (fixSpec & (FIX_GLOBAL_SUM | TRASH_GLOBAL_SUM)) {
		// Computation of the global checksum does not include the checksum bytes
		assume(rom0Len >= 0x14E);
		for (uint16_t i = 0; i < 0x14E; i++)
			globalSum += rom0[i];
		for (uint16_t i = 0x150; i < rom0Len; i++)
			globalSum += rom0[i];
		// Pipes have already read ROMX and updated globalSum, but not regular files
		if (input == output) {
			for (;;) {
				ssize_t bankLen = readBytes(input, bank, sizeof(bank));

				for (uint16_t i = 0; i < bankLen; i++)
					globalSum += bank[i];
				if (bankLen != sizeof(bank))
					break;
			}
		}

		if (fixSpec & TRASH_GLOBAL_SUM)
			globalSum = ~globalSum;

		uint8_t bytes[2] = {(uint8_t)(globalSum >> 8), (uint8_t)(globalSum & 0xFF)};

		overwriteBytes(rom0, 0x14E, bytes, sizeof(bytes), "global checksum");
	}

	ssize_t writeLen;

	// In case the output depends on the input, reset to the beginning of the file, and only
	// write the header
	if (input == output) {
		if (lseek(output, 0, SEEK_SET) == (off_t)-1) {
			report("FATAL: Failed to rewind \"%s\": %s\n", name, strerror(errno));
			return;
		}
		// If modifying the file in-place, we only need to edit the header
		// However, padding may have modified ROM0 (added padding), so don't in that case
		if (padValue == UNSPECIFIED)
			rom0Len = headerSize;
	}
	writeLen = writeBytes(output, rom0, rom0Len);

	if (writeLen == -1) {
		report("FATAL: Failed to write \"%s\"'s ROM0: %s\n", name, strerror(errno));
		return;
	} else if (writeLen < rom0Len) {
		report(
		    "FATAL: Could only write %jd of \"%s\"'s %jd ROM0 bytes\n",
		    (intmax_t)writeLen,
		    name,
		    (intmax_t)rom0Len
		);
		return;
	}

	// Output ROMX if it was buffered
	if (!romx.empty()) {
		// The value returned is either -1, or smaller than `totalRomxLen`,
		// so it's fine to cast to `size_t`
		writeLen = writeBytes(output, romx.data(), totalRomxLen);
		if (writeLen == -1) {
			report("FATAL: Failed to write \"%s\"'s ROMX: %s\n", name, strerror(errno));
			return;
		} else if ((size_t)writeLen < totalRomxLen) {
			report(
			    "FATAL: Could only write %jd of \"%s\"'s %zu ROMX bytes\n",
			    (intmax_t)writeLen,
			    name,
			    totalRomxLen
			);
			return;
		}
	}

	// Output padding
	if (padValue != UNSPECIFIED) {
		if (input == output) {
			if (lseek(output, 0, SEEK_END) == (off_t)-1) {
				report("FATAL: Failed to seek to end of \"%s\": %s\n", name, strerror(errno));
				return;
			}
		}
		memset(bank, padValue, sizeof(bank));
		size_t len = (nbBanks - 1) * BANK_SIZE - totalRomxLen; // Don't count ROM0!

		while (len) {
			static_assert(sizeof(bank) <= SSIZE_MAX, "Bank too large for reading");
			size_t thisLen = len > sizeof(bank) ? sizeof(bank) : len;
			ssize_t ret = writeBytes(output, bank, thisLen);

			// The return value is either -1, or at most `thisLen`,
			// so it's fine to cast to `size_t`
			if ((size_t)ret != thisLen) {
				report("FATAL: Failed to write \"%s\"'s padding: %s\n", name, strerror(errno));
				break;
			}
			len -= thisLen;
		}
	}
}

static bool processFilename(char const *name) {
	nbErrors = 0;

	if (!strcmp(name, "-")) {
		(void)setmode(STDIN_FILENO, O_BINARY);
		(void)setmode(STDOUT_FILENO, O_BINARY);
		name = "<stdin>";
		processFile(STDIN_FILENO, STDOUT_FILENO, name, 0);
	} else {
		// POSIX specifies that the results of O_RDWR on a FIFO are undefined.
		// However, this is necessary to avoid a TOCTTOU, if the file was changed between
		// `stat()` and `open(O_RDWR)`, which could trigger the UB anyway.
		// Thus, we're going to hope that either the `open` fails, or it succeeds but IO
		// operations may fail, all of which we handle.
		if (int input = open(name, O_RDWR | O_BINARY); input == -1) {
			report("FATAL: Failed to open \"%s\" for reading+writing: %s\n", name, strerror(errno));
		} else {
			Defer closeInput{[&] { close(input); }};
			struct stat stat;
			if (fstat(input, &stat) == -1) {
				report("FATAL: Failed to stat \"%s\": %s\n", name, strerror(errno));
			} else if (!S_ISREG(stat.st_mode)) { // TODO: Do we want to support other types?
				report(
				    "FATAL: \"%s\" is not a regular file, and thus cannot be modified in-place\n",
				    name
				);
			} else if (stat.st_size < 0x150) {
				// This check is in theory redundant with the one in `processFile`, but it
				// prevents passing a file size of 0, which usually indicates pipes
				report(
				    "FATAL: \"%s\" too short, expected at least 336 ($150) bytes, got only %jd\n",
				    name,
				    (intmax_t)stat.st_size
				);
			} else {
				processFile(input, input, name, stat.st_size);
			}
		}
	}

	if (nbErrors)
		fprintf(
		    stderr,
		    "Fixing \"%s\" failed with %u error%s\n",
		    name,
		    nbErrors,
		    nbErrors == 1 ? "" : "s"
		);
	return nbErrors;
}

static void parseByte(uint16_t &output, char name) {
	if (musl_optarg[0] == 0) {
		report("error: Argument to option '%c' may not be empty\n", name);
	} else {
		char *endptr;
		unsigned long value;

		if (musl_optarg[0] == '$') {
			value = strtoul(&musl_optarg[1], &endptr, 16);
		} else {
			value = strtoul(musl_optarg, &endptr, 0);
		}
		if (*endptr) {
			report(
			    "error: Expected number as argument to option '%c', got %s\n", name, musl_optarg
			);
		} else if (value > 0xFF) {
			report("error: Argument to option '%c' is larger than 255: %lu\n", name, value);
		} else {
			output = value;
		}
	}
}

int main(int argc, char *argv[]) {
	nbErrors = 0;

	for (int ch; (ch = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1;) {
		switch (ch) {
			size_t len;

		case 'C':
		case 'c':
			model = ch == 'c' ? BOTH : CGB;
			if (titleLen > 15) {
				titleLen = 15;
				fprintf(stderr, "warning: Truncating title \"%s\" to 15 chars\n", title);
			}
			break;

		case 'f':
			fixSpec = 0;
			while (*musl_optarg) {
				switch (*musl_optarg) {
#define OVERRIDE_SPEC(cur, bad, curFlag, badFlag) \
	case STR(cur)[0]: \
		if (fixSpec & badFlag) \
			fprintf(stderr, "warning: '" STR(cur) "' overriding '" STR(bad) "' in fix spec\n"); \
		fixSpec = (fixSpec & ~badFlag) | curFlag; \
		break
#define overrideSpecs(fix, fixFlag, trash, trashFlag) \
	OVERRIDE_SPEC(fix, trash, fixFlag, trashFlag); \
	OVERRIDE_SPEC(trash, fix, trashFlag, fixFlag)
				overrideSpecs(l, FIX_LOGO,       L, TRASH_LOGO);
				overrideSpecs(h, FIX_HEADER_SUM, H, TRASH_HEADER_SUM);
				overrideSpecs(g, FIX_GLOBAL_SUM, G, TRASH_GLOBAL_SUM);
#undef OVERRIDE_SPEC
#undef overrideSpecs

				default:
					fprintf(stderr, "warning: Ignoring '%c' in fix spec\n", *musl_optarg);
				}
				musl_optarg++;
			}
			break;

		case 'i':
			gameID = musl_optarg;
			len = strlen(gameID);
			if (len > 4) {
				len = 4;
				fprintf(stderr, "warning: Truncating game ID \"%s\" to 4 chars\n", gameID);
			}
			gameIDLen = len;
			if (titleLen > 11) {
				titleLen = 11;
				fprintf(stderr, "warning: Truncating title \"%s\" to 11 chars\n", title);
			}
			break;

		case 'j':
			japanese = false;
			break;

		case 'k':
			newLicensee = musl_optarg;
			len = strlen(newLicensee);
			if (len > 2) {
				len = 2;
				fprintf(
				    stderr, "warning: Truncating new licensee \"%s\" to 2 chars\n", newLicensee
				);
			}
			newLicenseeLen = len;
			break;

		case 'L':
			logoFilename = musl_optarg;
			break;

		case 'l':
			parseByte(oldLicensee, 'l');
			break;

		case 'm':
			cartridgeType = parseMBC(musl_optarg);
			if (cartridgeType == MBC_BAD) {
				report("error: Unknown MBC \"%s\"\nAccepted MBC names:\n", musl_optarg);
				printAcceptedMBCNames();
			} else if (cartridgeType == MBC_WRONG_FEATURES) {
				report(
				    "error: Features incompatible with MBC (\"%s\")\nAccepted combinations:\n",
				    musl_optarg
				);
				printAcceptedMBCNames();
			} else if (cartridgeType == MBC_BAD_RANGE) {
				report("error: Specified MBC ID out of range 0-255: %s\n", musl_optarg);
			} else if (cartridgeType == ROM_RAM || cartridgeType == ROM_RAM_BATTERY) {
				fprintf(
				    stderr,
				    "warning: ROM+RAM / ROM+RAM+BATTERY are under-specified and poorly "
				    "supported\n"
				);
			}
			break;

		case 'n':
			parseByte(romVersion, 'n');
			break;

		case 'O':
			overwriteRom = true;
			break;

		case 'p':
			parseByte(padValue, 'p');
			break;

		case 'r':
			parseByte(ramSize, 'r');
			break;

		case 's':
			sgb = true;
			break;

		case 't': {
			title = musl_optarg;
			len = strlen(title);
			uint8_t maxLen = maxTitleLen();

			if (len > maxLen) {
				len = maxLen;
				fprintf(stderr, "warning: Truncating title \"%s\" to %u chars\n", title, maxLen);
			}
			titleLen = len;
			break;
		}

		case 'V':
			printf("rgbfix %s\n", get_package_version_string());
			exit(0);

		case 'v':
			fixSpec = FIX_LOGO | FIX_HEADER_SUM | FIX_GLOBAL_SUM;
			break;

		default:
			printUsage();
			exit(1);
		}
	}

	if ((cartridgeType & 0xFF00) == TPP1 && !japanese)
		fprintf(
		    stderr,
		    "warning: TPP1 overwrites region flag for its identification code, ignoring `-j`\n"
		);

	// Check that RAM size is correct for "standard" mappers
	if (ramSize != UNSPECIFIED && (cartridgeType & 0xFF00) == 0) {
		if (cartridgeType == ROM_RAM || cartridgeType == ROM_RAM_BATTERY) {
			if (ramSize != 1)
				fprintf(
				    stderr,
				    "warning: MBC \"%s\" should have 2 KiB of RAM (-r 1)\n",
				    mbcName(cartridgeType)
				);
		} else if (hasRAM(cartridgeType)) {
			if (!ramSize) {
				fprintf(
				    stderr,
				    "warning: MBC \"%s\" has RAM, but RAM size was set to 0\n",
				    mbcName(cartridgeType)
				);
			} else if (ramSize == 1) {
				fprintf(
				    stderr,
				    "warning: RAM size 1 (2 KiB) was specified for MBC \"%s\"\n",
				    mbcName(cartridgeType)
				);
			} // TODO: check possible values?
		} else if (ramSize) {
			fprintf(
			    stderr,
			    "warning: MBC \"%s\" has no RAM, but RAM size was set to %u\n",
			    mbcName(cartridgeType),
			    ramSize
			);
		}
	}

	if (sgb && oldLicensee != UNSPECIFIED && oldLicensee != 0x33)
		fprintf(
		    stderr,
		    "warning: SGB compatibility enabled, but old licensee is 0x%02x, not 0x33\n",
		    oldLicensee
		);

	argv += musl_optind;
	bool failed = nbErrors;

	if (logoFilename) {
		FILE *logoFile;
		if (strcmp(logoFilename, "-")) {
			logoFile = fopen(logoFilename, "rb");
		} else {
			logoFilename = "<stdin>";
			logoFile = fdopen(STDIN_FILENO, "rb");
		}
		if (!logoFile) {
			fprintf(
			    stderr,
			    "FATAL: Failed to open \"%s\" for reading: %s\n",
			    logoFilename,
			    strerror(errno)
			);
			exit(1);
		}
		Defer closeLogo{[&] { fclose(logoFile); }};
		uint8_t logoBpp[sizeof(logo)];
		if (size_t nbRead = fread(logoBpp, 1, sizeof(logoBpp), logoFile);
		    nbRead != sizeof(logo) || fgetc(logoFile) != EOF || ferror(logoFile)) {
			fprintf(stderr, "FATAL: \"%s\" is not %zu bytes\n", logoFilename, sizeof(logo));
			exit(1);
		}
		auto highs = [&logoBpp](size_t i) {
			return (logoBpp[i * 2] & 0xF0) | ((logoBpp[i * 2 + 1] & 0xF0) >> 4);
		};
		auto lows = [&logoBpp](size_t i) {
			return ((logoBpp[i * 2] & 0x0F) << 4) | (logoBpp[i * 2 + 1] & 0x0F);
		};
		constexpr size_t mid = sizeof(logo) / 2;
		for (size_t i = 0; i < mid; i += 4) {
			logo[i + 0] = highs(i + 0);
			logo[i + 1] = highs(i + 1);
			logo[i + 2] = lows(i + 0);
			logo[i + 3] = lows(i + 1);
			logo[mid + i + 0] = highs(i + 2);
			logo[mid + i + 1] = highs(i + 3);
			logo[mid + i + 2] = lows(i + 2);
			logo[mid + i + 3] = lows(i + 3);
		}
	} else {
		memcpy(logo, nintendoLogo, sizeof(nintendoLogo));
	}
	if (fixSpec & TRASH_LOGO) {
		for (uint16_t i = 0; i < sizeof(logo); i++)
			logo[i] = 0xFF ^ logo[i];
	}

	if (!*argv) {
		fputs(
		    "FATAL: Please specify an input file (pass `-` to read from standard input)\n", stderr
		);
		printUsage();
		exit(1);
	}

	do {
		failed |= processFilename(*argv);
	} while (*++argv);

	return failed;
}
