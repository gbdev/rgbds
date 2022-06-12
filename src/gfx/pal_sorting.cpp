
#include "gfx/pal_sorting.hpp"

#include <algorithm>
#include <png.h>
#include <vector>

#include "helpers.h"

#include "gfx/convert.hpp"
#include "gfx/main.hpp"

using std::swap;

namespace sorting {

void indexed(std::vector<Palette> &palettes, int palSize, png_color const *palRGB,
             png_byte *palAlpha) {
	options.verbosePrint("Sorting palettes using embedded palette...\n");

	auto pngToRgb = [&palRGB, &palAlpha](int index) {
		auto const &c = palRGB[index];
		return Rgba(c.red, c.green, c.blue, palAlpha ? palAlpha[index] : 0xFF);
	};

	// HACK: for compatibility with old versions, add unused colors if:
	// - there is only one palette, and
	// - only some of the first N colors are being used
	if (palettes.size() == 1) {
		Palette &palette = palettes[0];
		// Build our candidate array of colors
		decltype(palette.colors) colors{UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
		for (int i = 0; i < options.maxPalSize(); ++i) {
			colors[i] = pngToRgb(i).cgbColor();
		}

		// Check that the palette only uses those colors
		if (std::all_of(palette.begin(), palette.end(), [&colors](uint16_t color) {
				return std::find(colors.begin(), colors.end(), color) != colors.end();
			})) {
			if (palette.size() != options.maxPalSize()) {
				warning("Unused color in PNG embedded palette was re-added; please use `-c embedded` to get this in future versions");
			}
			// Overwrite the palette, and return with that (it's already sorted)
			palette.colors = colors;
			return;
		}
	}

	for (Palette &pal : palettes) {
		std::sort(pal.begin(), pal.end(), [&](uint16_t lhs, uint16_t rhs) {
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
			unreachable_(); // This should not be possible
		});
	}
}

void grayscale(std::vector<Palette> &palettes,
               std::array<std::optional<Rgba>, 0x8001> const &colors) {
	options.verbosePrint("Sorting grayscale-only palette...\n");

	// This method is only applicable if there are at most as many colors as colors per palette, so
	// we should only have a single palette.
	assert(palettes.size() == 1);

	Palette &palette = palettes[0];
	std::fill(palette.begin(), palette.end(), Rgba::transparent);
	for (auto const &slot : colors) {
		if (!slot.has_value() || slot->isTransparent()) {
			continue;
		}
		palette[slot->grayIndex()] = slot->cgbColor();
	}
}

static unsigned int legacyLuminance(uint16_t color) {
	uint8_t red = color & 0b11111;
	uint8_t green = color >> 5 & 0b11111;
	uint8_t blue = color >> 10;
	return 2126 * red + 7152 * green + 722 * blue;
}

void rgb(std::vector<Palette> &palettes) {
	options.verbosePrint("Sorting palettes by \"\"\"luminance\"\"\"...\n");

	for (Palette &pal : palettes) {
		std::sort(pal.begin(), pal.end(), [](uint16_t lhs, uint16_t rhs) {
			return legacyLuminance(lhs) < legacyLuminance(rhs);
		});
	}
}

} // namespace sorting
