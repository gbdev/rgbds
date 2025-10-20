// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_MAIN_HPP
#define RGBDS_GFX_MAIN_HPP

#include <array>
#include <optional>
#include <stdint.h>
#include <string>
#include <vector>

#include "helpers.hpp" // assume

#include "gfx/rgba.hpp"

struct Options {
	bool useColorCurve = false;   // -C
	bool allowDedup = false;      // -u
	bool allowMirroringX = false; // -X, -m
	bool allowMirroringY = false; // -Y, -m
	bool columnMajor = false;     // -Z

	std::string attrmap{};                    // -a, -A
	std::optional<Rgba> bgColor{};            // -B
	std::array<uint8_t, 2> baseTileIDs{0, 0}; // -b
	enum {
		NO_SPEC,
		EXPLICIT,
		EMBEDDED,
		DMG,
	} palSpecType = NO_SPEC; // -c
	std::vector<std::array<std::optional<Rgba>, 4>> palSpec{};
	uint8_t palSpecDmg = 0;
	uint8_t bitDepth = 2;       // -d
	std::string inputTileset{}; // -i
	struct {
		uint16_t left;
		uint16_t top;
		uint16_t width;
		uint16_t height;
		uint32_t right() const { return left + width * 8; }
		uint32_t bottom() const { return top + height * 8; }
	} inputSlice{0, 0, 0, 0};                          // -L (margins in clockwise order, like CSS)
	uint8_t basePalID = 0;                             // -l
	std::array<uint16_t, 2> maxNbTiles{UINT16_MAX, 0}; // -N
	uint16_t nbPalettes = 8;                           // -n
	std::string output{};                              // -o
	std::string palettes{};                            // -p, -P
	std::string palmap{};                              // -q, -Q
	uint16_t reversedWidth = 0;                        // -r, in tiles
	uint8_t nbColorsPerPal = 0;                        // -s; 0 means "auto" = 1 << bitDepth;
	std::string tilemap{};                             // -t, -T
	uint64_t trim = 0;                                 // -x

	std::string input{}; // positional arg

	mutable bool hasTransparentPixels = false;
	uint8_t maxOpaqueColors() const { return nbColorsPerPal - hasTransparentPixels; }

	uint16_t maxNbColors() const { return nbColorsPerPal * nbPalettes; }

	uint8_t dmgColors[4] = {};
	uint8_t dmgValue(uint8_t i) const {
		assume(i < 4);
		return (palSpecDmg >> (2 * i)) & 0b11;
	}
};

extern Options options;

#endif // RGBDS_GFX_MAIN_HPP
