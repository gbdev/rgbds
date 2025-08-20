// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PNG_HPP
#define RGBDS_GFX_PNG_HPP

#include <stdint.h>
#include <streambuf>
#include <vector>

#include "gfx/rgba.hpp"

struct Png {
	uint32_t width, height;
	std::vector<Rgba> pixels{};
	std::vector<Rgba> palette{};

	Png() {}
	Png(char const *filename, std::streambuf &file);
};

#endif // RGBDS_GFX_PNG_HPP
