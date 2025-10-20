// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PALETTE_HPP
#define RGBDS_GFX_PALETTE_HPP

#include <array>
#include <stddef.h>
#include <stdint.h>

struct Palette {
	// An array of 4 GBC-native (RGB555) colors
	std::array<uint16_t, 4> colors{UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};

	void addColor(uint16_t color);
	uint8_t indexOf(uint16_t color) const;
	uint16_t &operator[](size_t index) { return colors[index]; }
	uint16_t const &operator[](size_t index) const { return colors[index]; }

	decltype(colors)::iterator begin();
	decltype(colors)::iterator end();
	decltype(colors)::const_iterator begin() const;
	decltype(colors)::const_iterator end() const;

	uint8_t size() const;
};

#endif // RGBDS_GFX_PALETTE_HPP
