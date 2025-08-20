// SPDX-License-Identifier: MIT

#include "verbosity.hpp"

#include <array>
#include <bitset>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "style.hpp"

static Verbosity verbosity = VERB_NONE;

bool checkVerbosity(Verbosity level) {
	return verbosity >= level;
}

// LCOV_EXCL_START

void incrementVerbosity() {
	if (verbosity < VERB_VVVVVV) {
		verbosity = static_cast<Verbosity>(verbosity + 1);
	}
}

void printVVVVVVerbosity() {
	if (!checkVerbosity(VERB_VVVVVV)) {
		return;
	}

	style_Set(stderr, STYLE_CYAN, true); // "Viridian"

	putc('\n', stderr);
	// clang-format off: vertically align values
	static std::array<std::bitset<10>, 21> gfx{
	    0b0111111110,
	    0b1111111111,
	    0b1110011001,
	    0b1110011001,
	    0b1111111111,
	    0b1111111111,
	    0b1110000001,
	    0b1111000011,
	    0b0111111110,
	    0b0001111000,
	    0b0111111110,
	    0b1111111111,
	    0b1111111111,
	    0b1111111111,
	    0b1101111011,
	    0b1101111011,
	    0b0011111100,
	    0b0011001100,
	    0b0111001110,
	    0b0111001110,
	    0b0111001110,
	};
	// clang-format on
	static std::array<char const *, 3> textbox{
	    "  ,----------------------------------------.",
	    "  | Augh, dimensional interference again?! |",
	    "  `----------------------------------------'",
	};
	for (size_t i = 0; i < gfx.size(); ++i) {
		std::bitset<10> const &row = gfx[i];
		for (uint8_t j = row.size(); j--;) {
			// Double the pixel horizontally, otherwise the aspect ratio looks wrong
			fputs(row[j] ? "00" : "  ", stderr);
		}
		if (i < textbox.size()) {
			fputs(textbox[i], stderr);
		}
		putc('\n', stderr);
	}
	putc('\n', stderr);

	style_Set(stderr, STYLE_MAGENTA, false);
}

// LCOV_EXCL_STOP
