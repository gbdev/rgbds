/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_GFX_MAIN_HPP
#define RGBDS_GFX_MAIN_HPP

#include <array>
#include <filesystem>
#include <limits.h>
#include <stdint.h>

#include "helpers.h"

struct Options {
	bool beVerbose = false; // -v
	bool fixInput = false; // -f
	bool columnMajor = false; // -h; whether to output the tilemap in columns instead of rows
	bool allowMirroring = false; // -m
	bool allowDedup = false; // -u
	bool useColorCurve = false; // -C
	uint8_t bitDepth = 2; // -d
	uint64_t trim = 0; // -x
	uint8_t nbPalettes = 8; // TODO
	uint8_t nbColorsPerPal = 0; // TODO; 0 means "auto" = 1 << bitDepth;
	std::array<uint8_t, 2> baseTileIDs{0, 0}; // TODO
	std::array<uint16_t, 2> maxNbTiles{UINT16_MAX, 0}; // TODO
	std::filesystem::path tilemap{}; // -t, -T
	std::filesystem::path attrmap{}; // -a, -A
	std::filesystem::path palettes{}; // -p, -P
	std::filesystem::path output{}; // -o
	std::filesystem::path input{}; // positional arg

	format_(printf, 2, 3) void verbosePrint(char const *fmt, ...) const;
	uint8_t maxPalSize() const {
		return nbColorsPerPal;
	} // TODO: minus 1 when transparency is active
};

extern Options options;

void warning(char const *fmt, ...);
void error(char const *fmt, ...);
[[noreturn]] void fatal(char const *fmt, ...);

struct Palette {
	// An array of 4 GBC-native (RGB555) colors
	std::array<uint16_t, 4> colors{UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};

	void addColor(uint16_t color);
	uint8_t indexOf(uint16_t color) const;

	decltype(colors)::iterator begin();
	decltype(colors)::iterator end();
	decltype(colors)::const_iterator begin() const;
	decltype(colors)::const_iterator end() const;
};

#endif /* RGBDS_GFX_MAIN_HPP */
