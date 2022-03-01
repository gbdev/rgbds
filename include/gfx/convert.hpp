/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_GFX_CONVERT_HPP
#define RGBDS_GFX_CONVERT_HPP

#include <assert.h>
#include <stdint.h>

#include "gfx/main.hpp"

struct Rgba {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;

	Rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : red(r), green(g), blue(b), alpha(a) {}
	Rgba(uint32_t rgba) : red(rgba), green(rgba >> 8), blue(rgba >> 16), alpha(rgba >> 24) {}

	operator uint32_t() const { return toCSS(); }
	/**
	 * Returns this RGBA as a 32-bit number that can be printed in hex (`%08x`) to yield its CSS
	 * representation
	 */
	uint32_t toCSS() const {
		auto shl = [](uint8_t val, unsigned shift) { return static_cast<uint32_t>(val) << shift; };
		return shl(red, 24) | shl(green, 16) | shl(blue, 8) | shl(alpha, 0);
	}
	bool operator!=(Rgba const &other) const {
		return static_cast<uint32_t>(*this) != static_cast<uint32_t>(other);
	}

	bool isGray() const { return red == green && green == blue; }

	/**
	 * CGB colors are RGB555, so we use bit 15 to signify that the color is transparent instead
	 * Since the rest of the bits don't matter then, we return 0x8000 exactly.
	 */
	static constexpr uint16_t transparent = 0b1'00000'00000'00000;

	/**
	 * All alpha values strictly below this will be considered transparent
	 */
	static constexpr uint8_t opacity_threshold = 0xF0; // TODO: adjust this
	/**
	 * Computes the equivalent CGB color, respects the color curve depending on options
	 */
	uint16_t cgbColor() const {
		if (alpha < opacity_threshold)
			return transparent;
		if (options.useColorCurve) {
			assert(!"TODO");
		} else {
			return (red >> 3) | (green >> 3) << 5 | (blue >> 3) << 10;
		}
	}
};

void process();

#endif /* RGBDS_GFX_CONVERT_HPP */
