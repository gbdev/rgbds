// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PNG_HPP
#define RGBDS_GFX_PNG_HPP

#include <stdint.h>
#include <streambuf>
#include <string>
#include <vector>

#include "gfx/rgba.hpp"

struct Png {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<Rgba> pixels{};
	std::vector<Rgba> palette{};
	bool isIndexed = false;

	Png() {}
	Png(std::string const &path);
	Png(char const *filename, std::streambuf &file);

private:
	void initialize(char const *filename, std::streambuf &file);
};

#endif // RGBDS_GFX_PNG_HPP
