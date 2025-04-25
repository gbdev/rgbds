// SPDX-License-Identifier: MIT

#include "gfx/pal_sorting.hpp"

#include <algorithm>

#include "helpers.hpp"

#include "gfx/main.hpp"

void sortIndexed(
    std::vector<Palette> &palettes,
    int palSize,
    png_color const *palRGB,
    int palAlphaSize,
    png_byte *palAlpha
) {
	options.verbosePrint(Options::VERB_LOG_ACT, "Sorting palettes using embedded palette...\n");

	auto pngToRgb = [&palRGB, &palAlphaSize, &palAlpha](int index) {
		auto const &c = palRGB[index];
		return Rgba(
		    c.red, c.green, c.blue, palAlpha && index < palAlphaSize ? palAlpha[index] : 0xFF
		);
	};

	for (Palette &pal : palettes) {
		std::sort(RANGE(pal), [&](uint16_t lhs, uint16_t rhs) {
			// Iterate through the PNG's palette, looking for either of the two
			for (int i = 0; i < palSize; ++i) {
				uint16_t color = pngToRgb(i).cgbColor();
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
	options.verbosePrint(Options::VERB_LOG_ACT, "Sorting palette by grayscale bins...\n");

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
	options.verbosePrint(Options::VERB_LOG_ACT, "Sorting palettes by luminance...\n");

	for (Palette &pal : palettes) {
		std::sort(RANGE(pal), [](uint16_t lhs, uint16_t rhs) {
			return luminance(lhs) > luminance(rhs);
		});
	}
}
