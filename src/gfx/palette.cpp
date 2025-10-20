// SPDX-License-Identifier: MIT

#include "gfx/palette.hpp"

#include <algorithm>
#include <stdint.h>

#include "helpers.hpp"

#include "gfx/main.hpp"
#include "gfx/rgba.hpp"

void Palette::addColor(uint16_t color) {
	for (size_t i = 0; true; ++i) {
		assume(i < colors.size()); // The packing should guarantee this
		if (colors[i] == color) {  // The color is already present
			break;
		} else if (colors[i] == UINT16_MAX) { // Empty slot
			colors[i] = color;
			break;
		}
	}
}

// Returns the ID of the color in the palette, or `size()` if the color is not in
uint8_t Palette::indexOf(uint16_t color) const {
	return color == Rgba::transparent
	           ? 0
	           : std::find(begin(), colors.end(), color) - begin() + options.hasTransparentPixels;
}

auto Palette::begin() -> decltype(colors)::iterator {
	// Skip the first slot if reserved for transparency
	return colors.begin() + options.hasTransparentPixels;
}

auto Palette::end() -> decltype(colors)::iterator {
	// Return an iterator pointing past the last non-empty element.
	// Since the palette may contain gaps, we must scan from the end.
	return std::find_if(RRANGE(colors), [](uint16_t c) { return c != UINT16_MAX; }).base();
}

auto Palette::begin() const -> decltype(colors)::const_iterator {
	// Same as the non-const begin().
	return colors.begin() + options.hasTransparentPixels;
}

auto Palette::end() const -> decltype(colors)::const_iterator {
	// Same as the non-const end().
	return std::find_if(RRANGE(colors), [](uint16_t c) { return c != UINT16_MAX; }).base();
}

uint8_t Palette::size() const {
	return end() - colors.begin();
}
