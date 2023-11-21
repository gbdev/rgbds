/* SPDX-License-Identifier: MIT */

#include "gfx/rgba.hpp"

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "gfx/main.hpp" // options

/*
 * Based on inverting the "Modern - Accurate" formula used by SameBoy
 * since commit b5a611c5db46d6a0649d04d24d8d6339200f9ca1 (Dec 2020),
 * with gaps in the scale curve filled by polynomial interpolation.
 */
static std::array<uint8_t, 256> reverse_curve{
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1, // These
    1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3, // comments
    3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,  4,  5,  5,  5,  5, // prevent
    5,  5,  5,  6,  6,  6,  6,  6,  6,  7,  7,  7,  7,  7,  7,  7, // clang-format
    7,  8,  8,  8,  8,  8,  8,  9,  9,  9,  9,  9, 10, 10, 10, 10, // from
    10, 10, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, // reflowing
    13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 16, 16, 16, // these
    16, 16, 16, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 19, 19, // sixteen
    19, 19, 19, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21, 21, 21, 22, // 16-item
    22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, // lines,
    24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, // which,
    26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, // in
    28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, // my
    29, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, // opinion,
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, // help
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, // visualization!
};

uint16_t Rgba::cgbColor() const {
	if (isTransparent()) {
		return transparent;
	}
	assert(isOpaque());

	uint8_t r = red, g = green, b = blue;
	if (options.useColorCurve) {
		double g_linear = pow(g / 255.0, 2.2), b_linear = pow(b / 255.0, 2.2);
		double g_adjusted = std::clamp((g_linear * 4 - b_linear) / 3, 0.0, 1.0);
		g = round(pow(g_adjusted, 1 / 2.2) * 255);
		r = reverse_curve[r];
		g = reverse_curve[g];
		b = reverse_curve[b];
	} else {
		r >>= 3;
		g >>= 3;
		b >>= 3;
	}
	return r | g << 5 | b << 10;
}

uint8_t Rgba::grayIndex() const {
	assert(isGray());
	// Convert from [0; 256[ to [0; maxOpaqueColors[
	return static_cast<uint16_t>(255 - red) * options.maxOpaqueColors() / 256;
}
