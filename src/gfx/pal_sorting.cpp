// SPDX-License-Identifier: MIT

#include "gfx/pal_sorting.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <stdint.h>
#include <vector>

#include "helpers.hpp"
#include "verbosity.hpp"

#include "gfx/main.hpp"
#include "gfx/rgba.hpp"

void sortIndexed(std::vector<Palette> &palettes, std::vector<Rgba> const &embPal) {
	verbosePrint(VERB_NOTICE, "Sorting palettes using embedded palette...\n");

	for (Palette &pal : palettes) {
		std::sort(RANGE(pal), [&](uint16_t lhs, uint16_t rhs) {
			// Iterate through the PNG's palette, looking for either of the two
			for (Rgba const &rgba : embPal) {
				uint16_t color = rgba.cgbColor();
				if (color == Rgba::transparent) {
					continue;
				}
				// Return whether lhs < rhs
				if (color == rhs) {
					return false;
				}
				if (color == lhs) {
					return true;
				}
			}
			unreachable_(); // LCOV_EXCL_LINE
		});
	}
}

void sortGrayscale(
    std::vector<Palette> &palettes, std::array<std::optional<Rgba>, NB_COLOR_SLOTS> const &colors
) {
	verbosePrint(VERB_NOTICE, "Sorting palette by grayscale bins...\n");

	// This method is only applicable if there are at most as many colors as colors per palette, so
	// we should only have a single palette.
	assume(palettes.size() == 1);

	Palette &palette = palettes[0];
	std::fill(RANGE(palette.colors), Rgba::transparent);
	for (std::optional<Rgba> const &slot : colors) {
		if (!slot.has_value() || slot->isTransparent()) {
			continue;
		}
		palette[slot->grayIndex()] = slot->cgbColor();
	}
}

static unsigned int luminance(uint16_t color) {
	uint8_t red = color & 0b11111;
	uint8_t green = color >> 5 & 0b11111;
	uint8_t blue = color >> 10;
	return 2126 * red + 7152 * green + 722 * blue;
}

void sortRgb(std::vector<Palette> &palettes) {
	verbosePrint(VERB_NOTICE, "Sorting palettes by luminance...\n");

	for (Palette &pal : palettes) {
		// Sort from lightest to darkest
		std::sort(RANGE(pal), [](uint16_t lhs, uint16_t rhs) {
			return luminance(lhs) > luminance(rhs);
		});
	}
}
