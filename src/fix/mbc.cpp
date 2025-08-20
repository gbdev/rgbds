// SPDX-License-Identifier: MIT

#include "fix/mbc.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <utility>

#include "helpers.hpp"  // unreachable_
#include "platform.hpp" // strcasecmp
#include "util.hpp"     // isBlankSpace, isDigit

#include "fix/warning.hpp"

// Associate every MBC type with its name and whether it has RAM
static std::unordered_map<MbcType, std::pair<char const *, bool>> mbcData{
    {ROM,                                   {"ROM", false}                           },
    {ROM_RAM,                               {"ROM+RAM", true}                        },
    {ROM_RAM_BATTERY,                       {"ROM+RAM+BATTERY", true}                },
    {MBC1,                                  {"MBC1", false}                          },
    {MBC1_RAM,                              {"MBC1+RAM", true}                       },
    {MBC1_RAM_BATTERY,                      {"MBC1+RAM+BATTERY", true}               },
    // MBC2 technically has RAM, but is not marked as such
    {MBC2,                                  {"MBC2", false}                          },
    {MBC2_BATTERY,                          {"MBC2+BATTERY", false}                  },
    {MMM01,                                 {"MMM01", false}                         },
    {MMM01_RAM,                             {"MMM01+RAM", true}                      },
    {MMM01_RAM_BATTERY,                     {"MMM01+RAM+BATTERY", true}              },
    {MBC3,                                  {"MBC3", false}                          },
    {MBC3_TIMER_BATTERY,                    {"MBC3+TIMER+BATTERY", false}            },
    {MBC3_TIMER_RAM_BATTERY,                {"MBC3+TIMER+RAM+BATTERY", true}         },
    {MBC3_RAM,                              {"MBC3+RAM", true}                       },
    {MBC3_RAM_BATTERY,                      {"MBC3+RAM+BATTERY", true}               },
    {MBC5,                                  {"MBC5", false}                          },
    {MBC5_RAM,                              {"MBC5+RAM", true}                       },
    {MBC5_RAM_BATTERY,                      {"MBC5+RAM+BATTERY", true}               },
    {MBC5_RUMBLE,                           {"MBC5+RUMBLE", false}                   },
    {MBC5_RUMBLE_RAM,                       {"MBC5+RUMBLE+RAM", true}                },
    {MBC5_RUMBLE_RAM_BATTERY,               {"MBC5+RUMBLE+RAM+BATTERY", true}        },
    // MBC6 "Net de Get - Minigame @ 100" has RAM size 3 (32 KiB)
    {MBC6,                                  {"MBC6", true}                           },
    {MBC7_SENSOR_RUMBLE_RAM_BATTERY,        {"MBC7+SENSOR+RUMBLE+RAM+BATTERY", true} },
    {POCKET_CAMERA,                         {"POCKET CAMERA", true}                  },
    // Bandai TAMA5 "Game de Hakken!! Tamagotchi - Osutchi to Mesutchi" has RAM size 0
    {BANDAI_TAMA5,                          {"BANDAI TAMA5", false}                  },
    {HUC3,                                  {"HUC3", true}                           },
    {HUC1_RAM_BATTERY,                      {"HUC1+RAM+BATTERY", true}               },
    // TPP1 may or may not have RAM, don't use these flags for it
    {TPP1,                                  {"TPP1", false}                          },
    {TPP1_RUMBLE,                           {"TPP1+RUMBLE", false}                   },
    {TPP1_MULTIRUMBLE_RUMBLE,               {"TPP1+MULTIRUMBLE", false}              },
    {TPP1_TIMER,                            {"TPP1+TIMER", false}                    },
    {TPP1_TIMER_RUMBLE,                     {"TPP1+TIMER+RUMBLE", false}             },
    {TPP1_TIMER_MULTIRUMBLE_RUMBLE,         {"TPP1+TIMER+MULTIRUMBLE", false}        },
    {TPP1_BATTERY,                          {"TPP1+BATTERY", false}                  },
    {TPP1_BATTERY_RUMBLE,                   {"TPP1+BATTERY+RUMBLE", false}           },
    {TPP1_BATTERY_MULTIRUMBLE_RUMBLE,       {"TPP1+BATTERY+MULTIRUMBLE", false}      },
    {TPP1_BATTERY_TIMER,                    {"TPP1+BATTERY+TIMER", false}            },
    {TPP1_BATTERY_TIMER_RUMBLE,             {"TPP1+BATTERY+TIMER+RUMBLE", false}     },
    {TPP1_BATTERY_TIMER_MULTIRUMBLE_RUMBLE, {"TPP1+BATTERY+TIMER+MULTIRUMBLE", false}},
};

static char const *acceptedMBCNames =
    "Accepted MBC names:\n"
    "\tROM ($00) [aka ROM_ONLY]\n"
    "\tMBC1 ($01), MBC1+RAM ($02), MBC1+RAM+BATTERY ($03)\n"
    "\tMBC2 ($05), MBC2+BATTERY ($06)\n"
    "\tROM+RAM ($08) [deprecated], ROM+RAM+BATTERY ($09) [deprecated]\n"
    "\tMMM01 ($0B), MMM01+RAM ($0C), MMM01+RAM+BATTERY ($0D)\n"
    "\tMBC3+TIMER+BATTERY ($0F), MBC3+TIMER+RAM+BATTERY ($10)\n"
    "\tMBC3 ($11), MBC3+RAM ($12), MBC3+RAM+BATTERY ($13)\n"
    "\tMBC5 ($19), MBC5+RAM ($1A), MBC5+RAM+BATTERY ($1B)\n"
    "\tMBC5+RUMBLE ($1C), MBC5+RUMBLE+RAM ($1D), MBC5+RUMBLE+RAM+BATTERY ($1E)\n"
    "\tMBC6 ($20)\n"
    "\tMBC7+SENSOR+RUMBLE+RAM+BATTERY ($22)\n"
    "\tPOCKET_CAMERA ($FC)\n"
    "\tBANDAI_TAMA5 ($FD) [aka TAMA5]\n"
    "\tHUC3 ($FE)\n"
    "\tHUC1+RAM+BATTERY ($FF)\n"
    "\n"
    "\tTPP1_1.0, TPP1_1.0+RUMBLE, TPP1_1.0+MULTIRUMBLE, TPP1_1.0+TIMER,\n"
    "\tTPP1_1.0+TIMER+RUMBLE, TPP1_1.0+TIMER+MULTIRUMBLE, TPP1_1.0+BATTERY,\n"
    "\tTPP1_1.0+BATTERY+RUMBLE, TPP1_1.0+BATTERY+MULTIRUMBLE,\n"
    "\tTPP1_1.0+BATTERY+TIMER, TPP1_1.0+BATTERY+TIMER+RUMBLE,\n"
    "\tTPP1_1.0+BATTERY+TIMER+MULTIRUMBLE"; // No trailing newline

char const *mbc_Name(MbcType type) {
	auto search = mbcData.find(type);
	return search != mbcData.end() ? search->second.first : "(unknown)";
}

bool mbc_HasRAM(MbcType type) {
	auto search = mbcData.find(type);
	return search != mbcData.end() && search->second.second;
}

static void skipBlankSpace(char const *&ptr) {
	while (isBlankSpace(*ptr)) {
		++ptr;
	}
}

static void skipMBCSpace(char const *&ptr) {
	while (isBlankSpace(*ptr) || *ptr == '_') {
		++ptr;
	}
}

static char normalizeMBCChar(char c) {
	if (c >= 'a' && c <= 'z') { // Uppercase for comparison with `mbc_Name`s
		c = c - 'a' + 'A';
	} else if (c == '_') { // Treat underscores as spaces
		c = ' ';
	}
	return c;
}

static bool readMBCSlice(char const *&name, char const *expected) {
	while (*expected) {
		// If `name` is too short, the character will be '\0' and this will return `false`
		if (normalizeMBCChar(*name++) != *expected++) {
			return false;
		}
	}
	return true;
}

[[noreturn]]
static void fatalUnknownMBC(char const *fullName) {
	fatal("Unknown MBC \"%s\"\n%s", fullName, acceptedMBCNames);
}

[[noreturn]]
static void fatalWrongMBCFeatures(char const *fullName) {
	fatal("Features incompatible with MBC (\"%s\")\n%s", fullName, acceptedMBCNames);
}

MbcType mbc_ParseName(char const *name, uint8_t &tpp1Major, uint8_t &tpp1Minor) {
	char const *fullName = name;

	if (!strcasecmp(name, "help") || !strcasecmp(name, "list")) {
		puts(acceptedMBCNames); // Outputs to stdout and appends a newline
		exit(0);
	}

	if (isDigit(name[0]) || name[0] == '$') {
		int base = 0;

		if (name[0] == '$') {
			++name;
			base = 16;
		}
		// Parse number, and return it as-is (unless it's too large)
		char *endptr;
		unsigned long mbc = strtoul(name, &endptr, base);

		if (*endptr) {
			fatalUnknownMBC(fullName);
		}
		if (mbc > 0xFF) {
			fatal("Specified MBC ID out of range 0-255: \"%s\"", fullName);
		}
		return static_cast<MbcType>(mbc);
	}

	// Begin by reading the MBC type:
	uint16_t mbc;
	char const *ptr = name;

	skipBlankSpace(ptr); // Trim off leading blank space

#define tryReadSlice(expected) \
	do { \
		if (!readMBCSlice(ptr, expected)) { \
			fatalUnknownMBC(fullName); \
		} \
	} while (0)

	switch (*ptr++) {
	case 'R': // ROM / ROM_ONLY
	case 'r':
		tryReadSlice("OM");
		// Handle optional " ONLY"
		skipMBCSpace(ptr);
		if (*ptr == 'O' || *ptr == 'o') {
			++ptr;
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
				fatalUnknownMBC(fullName);
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
				fatalUnknownMBC(fullName);
			}
			break;
		case 'M':
		case 'm':
			tryReadSlice("M01");
			mbc = MMM01;
			break;
		default:
			fatalUnknownMBC(fullName);
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
			skipMBCSpace(ptr);
			// Major
			char *endptr;
			unsigned long val = strtoul(ptr, &endptr, 10);

			if (endptr == ptr) {
				fatal("Failed to parse TPP1 major revision number");
			}
			ptr = endptr;
			if (val != 1) {
				fatal("RGBFIX only supports TPP1 version 1.0");
			}
			tpp1Major = val;
			tryReadSlice(".");
			// Minor
			val = strtoul(ptr, &endptr, 10);
			if (endptr == ptr) {
				fatal("Failed to parse TPP1 minor revision number");
			}
			ptr = endptr;
			if (val > 0xFF) {
				fatal("TPP1 minor revision number must be 8-bit");
			}
			tpp1Minor = val;
			mbc = TPP1;
			break;
		}
		default:
			fatalUnknownMBC(fullName);
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
			fatalUnknownMBC(fullName);
		}
		break;

	default:
		fatalUnknownMBC(fullName);
	}

	// Read "additional features"
	uint8_t features = 0;
	// clang-format off: vertically align values
	static constexpr uint8_t RAM         = 1 << 7;
	static constexpr uint8_t BATTERY     = 1 << 6;
	static constexpr uint8_t TIMER       = 1 << 5;
	static constexpr uint8_t RUMBLE      = 1 << 4;
	static constexpr uint8_t SENSOR      = 1 << 3;
	static constexpr uint8_t MULTIRUMBLE = 1 << 2;
	// clang-format on

	for (;;) {
		skipBlankSpace(ptr); // Trim off trailing blank space

		// If done, start processing "features"
		if (!*ptr) {
			break;
		}
		// We expect a '+' at this point
		skipMBCSpace(ptr);
		if (*ptr++ != '+') {
			fatalUnknownMBC(fullName);
		}
		skipMBCSpace(ptr);

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
				tryReadSlice("M");
				features |= RAM;
				break;
			default:
				fatalUnknownMBC(fullName);
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
			fatalUnknownMBC(fullName);
		}
	}
#undef tryReadSlice

	switch (mbc) {
	case ROM:
		if (!features) {
			break;
		}
		mbc = ROM_RAM - 1;
		static_assert(ROM_RAM + 1 == ROM_RAM_BATTERY, "Enum sanity check failed!");
		static_assert(MBC1 + 1 == MBC1_RAM, "Enum sanity check failed!");
		static_assert(MBC1 + 2 == MBC1_RAM_BATTERY, "Enum sanity check failed!");
		static_assert(MMM01 + 1 == MMM01_RAM, "Enum sanity check failed!");
		static_assert(MMM01 + 2 == MMM01_RAM_BATTERY, "Enum sanity check failed!");
		[[fallthrough]];
	case MBC1:
	case MMM01:
		if (features == RAM) {
			++mbc;
		} else if (features == (RAM | BATTERY)) {
			mbc += 2;
		} else if (features) {
			fatalWrongMBCFeatures(fullName);
		}
		break;

	case MBC2:
		if (features == BATTERY) {
			mbc = MBC2_BATTERY;
		} else if (features) {
			fatalWrongMBCFeatures(fullName);
		}
		break;

	case MBC3:
		// Handle timer, which also requires battery
		if (features & TIMER) {
			if (!(features & BATTERY)) {
				warning(WARNING_MBC, "\"MBC3+TIMER\" implies \"BATTERY\"");
			}
			features &= ~(TIMER | BATTERY); // Reset those bits
			mbc = MBC3_TIMER_BATTERY;
			// RAM is handled below
		}
		static_assert(MBC3 + 1 == MBC3_RAM, "Enum sanity check failed!");
		static_assert(MBC3 + 2 == MBC3_RAM_BATTERY, "Enum sanity check failed!");
		static_assert(
		    MBC3_TIMER_BATTERY + 1 == MBC3_TIMER_RAM_BATTERY, "Enum sanity check failed!"
		);
		if (features == RAM) {
			++mbc;
		} else if (features == (RAM | BATTERY)) {
			mbc += 2;
		} else if (features) {
			fatalWrongMBCFeatures(fullName);
		}
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
		if (features == RAM) {
			++mbc;
		} else if (features == (RAM | BATTERY)) {
			mbc += 2;
		} else if (features) {
			fatalWrongMBCFeatures(fullName);
		}
		break;

	case MBC6:
	case POCKET_CAMERA:
	case BANDAI_TAMA5:
	case HUC3:
		// No extra features accepted
		if (features) {
			fatalWrongMBCFeatures(fullName);
		}
		break;

	case MBC7_SENSOR_RUMBLE_RAM_BATTERY:
		if (features != (SENSOR | RUMBLE | RAM | BATTERY)) {
			fatalWrongMBCFeatures(fullName);
		}
		break;

	case HUC1_RAM_BATTERY:
		if (features != (RAM | BATTERY)) { // HuC1 expects RAM+BATTERY
			fatalWrongMBCFeatures(fullName);
		}
		break;

	case TPP1:
		if (features & RAM) {
			warning(WARNING_MBC, "TPP1 requests RAM implicitly if given a non-zero RAM size");
		}
		if (features & BATTERY) {
			mbc |= 0x08;
		}
		if (features & TIMER) {
			mbc |= 0x04;
		}
		if (features & MULTIRUMBLE) {
			mbc |= 0x03; // Also set the rumble flag
		}
		if (features & RUMBLE) {
			mbc |= 0x01;
		}
		if (features & SENSOR) {
			fatalWrongMBCFeatures(fullName);
		}
		// Multiple rumble speeds imply rumble
		if (mbc & 0x01) {
			assume(mbc & 0x02);
		}
		break;
	}

	skipBlankSpace(ptr); // Trim off trailing blank space

	// If there is still something left, error out
	if (*ptr) {
		fatalUnknownMBC(fullName);
	}

	return static_cast<MbcType>(mbc);
}
