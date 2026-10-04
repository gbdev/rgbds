// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_MAIN_HPP
#define RGBDS_GFX_MAIN_HPP

#include <array>
#include <optional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "helpers.hpp" // assume

#include "gfx/rgba.hpp"

static constexpr size_t NB_BANKS = 2;

static constexpr uint32_t TILE_WIDTH = 8;  // in pixels
static constexpr uint32_t TILE_HEIGHT = 8; // in pixels

// Forward declaration so `inputSlice`'s `bottom` method inside `Options` can refer to `options`.
struct Options;
extern Options options;

struct Options {
	bool useColorCurve = false;   // -C
	bool oam = false;             // -j
	bool allowDedup = false;      // -u
	bool allowMirroringX = false; // -X, -m
	bool allowMirroringY = false; // -Y, -m
	bool columnMajor = false;     // -Z

	// An 8x16 px OAM object is two 8x8 px tiles stacked vertically.
	uint8_t nbIDsPerTile() const { return oam ? 2 : 1; }
	uint32_t tileHeight() const { return TILE_HEIGHT * nbIDsPerTile(); } // in pixels
	uint16_t maxNbTilesPerBank() const { return 256 / nbIDsPerTile(); }

	std::string attrmap{};                           // -a, -A
	std::optional<Rgba> bgColor{};                   // -B
	std::array<uint8_t, NB_BANKS> baseTileIDs{0, 0}; // -b
	enum {
		NO_SPEC,
		INLINE,
		EXTERNAL,
		EMBEDDED,
		EMBEDDED_MULTIPLE,
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
		uint32_t right() const { return left + width * TILE_WIDTH; }
		uint32_t bottom() const { return top + height * options.tileHeight(); }
	} inputSlice{0, 0, 0, 0};                                 // -L (clockwise margins like CSS)
	uint8_t basePalID = 0;                                    // -l
	std::array<uint16_t, NB_BANKS> maxNbTiles{UINT16_MAX, 0}; // -N
	uint16_t nbPalettes = 8;                                  // -n
	std::string output{};                                     // -o
	std::string palettes{};                                   // -p, -P
	std::string palmap{};                                     // -q, -Q
	uint16_t reversedWidth = 0;                               // -r, in tiles
	uint8_t nbColorsPerPal = 0;                               // -s; 0 means "auto" = 1 << bitDepth
	std::string tilemap{};                                    // -t, -T
	uint64_t trim = 0;                                        // -x

	std::string input{}; // positional arg

	mutable bool hasTransparentPixels = false;
	uint8_t maxOpaqueColors() const { return nbColorsPerPal - hasTransparentPixels; }

	uint16_t maxNbColors() const { return nbColorsPerPal * nbPalettes; }

	bool hasExplicitPalSpec() const { return palSpecType == INLINE || palSpecType == EXTERNAL; }
	bool hasEmbeddedPalSpec() const {
		return palSpecType == EMBEDDED || palSpecType == EMBEDDED_MULTIPLE;
	}

	size_t tileSize() const { return tileHeight() * bitDepth; } // in bytes

	uint8_t dmgColors[4] = {};
	uint8_t dmgValue(uint8_t i) const {
		assume(i < 4);
		return (palSpecDmg >> (2 * i)) & 0b11;
	}
};

#endif // RGBDS_GFX_MAIN_HPP
