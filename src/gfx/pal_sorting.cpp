
#include "gfx/pal_sorting.hpp"

#include <algorithm>
#include <png.h>
#include <vector>

#include "helpers.h"

#include "gfx/main.hpp"

namespace sorting {

void indexed(std::vector<Palette> &palettes, int palSize, png_color const *palRGB,
             png_byte *palAlpha) {
	options.verbosePrint("Sorting palettes using embedded palette...\n");

	for (Palette &pal : palettes) {
		std::sort(pal.begin(), pal.end(), [&](uint16_t lhs, uint16_t rhs) {
			// Iterate through the PNG's palette, looking for either of the two
			for (int i = 0; i < palSize; ++i) {
				if (palAlpha && palAlpha[i])
					continue;
				auto const &c = palRGB[i];
				uint16_t cgbColor = c.red >> 3 | (c.green >> 3) << 5 | (c.blue >> 3) << 10;
				// Return whether lhs < rhs
				if (cgbColor == rhs) {
					return false;
				}
				if (cgbColor == lhs) {
					return true;
				}
			}
			unreachable_(); // This should not be possible
		});
	}
}

void grayscale(std::vector<Palette> &palettes) {
	options.verbosePrint("Sorting grayscale-only palettes...\n");

	for (Palette &pal : palettes) {
		(void)pal; // TODO
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
