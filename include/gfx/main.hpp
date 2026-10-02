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

struct Options {
	bool useColorCurve = false;   // -C
	bool allowDedup = false;      // -u
	bool allowMirroringX = false; // -X, -m
	bool allowMirroringY = false; // -Y, -m
	bool columnMajor = false;     // -Z
	bool oam = false;             // -j

	std::string attrmap{};                    // -a, -A
	std::optional<Rgba> bgColor{};            // -B
	std::array<uint8_t, 2> baseTileIDs{0, 0}; // -b
	enum {
		NO_SPEC,
		INLINE,
		EXTERNAL,
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
		// The height counts tiles, which are twice as tall with `-j/--oam`
		uint32_t bottom(uint32_t tileHeight) const { return top + height * tileHeight; }
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

	bool hasExplicitPalSpec() const { return palSpecType == INLINE || palSpecType == EXTERNAL; }

	// Tiles are always 8 pixels wide, but they are either 8 or 16 pixels tall: the Game Boy's OAM
	// objects ("sprites") can be 8x16 instead of 8x8 (see `-j/--oam`).
	uint32_t tileHeight() const { return oam ? 16 : 8; }

	// How many 8x8 px tiles a tile is made of, which is also how many tile IDs it takes up in the
	// tilemap: an OAM object is two 8x8 px tiles stacked vertically.
	uint8_t nbTileIDs() const { return tileHeight() / 8; }

	// How many tiles fit in one VRAM bank; since each one still takes up `nbTileIDs()` tile IDs,
	// OAM objects only leave room for half as many of them.
	uint16_t maxNbTilesPerBank() const { return 256 / nbTileIDs(); }

	// How many bytes one tile takes up in the tile data file, at the output bit depth.
	size_t tileSize() const { return tileHeight() * bitDepth; }

	// How many bytes one tile takes up internally, where they are **always** 2bpp (see `TileData`).
	size_t tileDataSize() const { return tileHeight() * 2; }

	uint8_t dmgColors[4] = {};
	uint8_t dmgValue(uint8_t i) const {
		assume(i < 4);
		return (palSpecDmg >> (2 * i)) & 0b11;
	}
};

extern Options options;

#endif // RGBDS_GFX_MAIN_HPP
