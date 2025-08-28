// SPDX-License-Identifier: MIT

#ifndef RGBDS_FIX_MAIN_HPP
#define RGBDS_FIX_MAIN_HPP

#include <stdint.h>

#include "fix/mbc.hpp" // UNSPECIFIED, MbcType

// clang-format off: vertically align values
static constexpr uint8_t FIX_LOGO         = 1 << 7;
static constexpr uint8_t TRASH_LOGO       = 1 << 6;
static constexpr uint8_t FIX_HEADER_SUM   = 1 << 5;
static constexpr uint8_t TRASH_HEADER_SUM = 1 << 4;
static constexpr uint8_t FIX_GLOBAL_SUM   = 1 << 3;
static constexpr uint8_t TRASH_GLOBAL_SUM = 1 << 2;
// clang-format on

enum Model { DMG, BOTH, CGB };

struct Options {
	uint8_t fixSpec = 0;                // -f, -v
	Model model = DMG;                  // -C, -c
	bool japanese = true;               // -j
	uint16_t oldLicensee = UNSPECIFIED; // -l
	uint16_t romVersion = UNSPECIFIED;  // -n
	uint16_t padValue = UNSPECIFIED;    // -p
	uint16_t ramSize = UNSPECIFIED;     // -r
	bool sgb = false;                   // -s

	char const *gameID = nullptr; // -i
	uint8_t gameIDLen;

	char const *newLicensee = nullptr; // -k
	uint8_t newLicenseeLen;

	char const *logoFilename = nullptr; // -L
	uint8_t logo[48] = {};

	MbcType cartridgeType = MBC_NONE; // -m
	uint8_t tpp1Rev[2];

	char const *title = nullptr; // -t
	uint8_t titleLen;
};

extern Options options;

#endif // RGBDS_FIX_MAIN_HPP
