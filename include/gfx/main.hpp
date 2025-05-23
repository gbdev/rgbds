// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_MAIN_HPP
#define RGBDS_GFX_MAIN_HPP

#include <array>
#include <optional>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "helpers.hpp"

#include "gfx/rgba.hpp"

struct Options {
	bool useColorCurve = false;   // -C
	bool allowDedup = false;      // -u
	bool allowMirroringX = false; // -X, -m
	bool allowMirroringY = false; // -Y, -m
	bool columnMajor = false;     // -Z
	uint8_t verbosity = 0;        // -v

	std::string attrmap{};                    // -a, -A
	std::optional<Rgba> bgColor{};            // -B
	std::array<uint8_t, 2> baseTileIDs{0, 0}; // -b
	enum {
		NO_SPEC,
		EXPLICIT,
		EMBEDDED,
	} palSpecType = NO_SPEC; // -c
	std::vector<std::array<std::optional<Rgba>, 4>> palSpec{};
	uint8_t bitDepth = 2;       // -d
	std::string inputTileset{}; // -i
	struct {
		uint16_t left;
		uint16_t top;
		uint16_t width;
		uint16_t height;
	} inputSlice{0, 0, 0, 0};                          // -L (margins in clockwise order, like CSS)
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

	// clang-format off: vertically align values
	static constexpr uint8_t VERB_NONE    = 0; // Normal, no extra output
	static constexpr uint8_t VERB_CFG     = 1; // Print configuration after parsing options
	static constexpr uint8_t VERB_LOG_ACT = 2; // Log actions before doing them
	static constexpr uint8_t VERB_INTERM  = 3; // Print some intermediate results
	static constexpr uint8_t VERB_DEBUG   = 4; // Internals are logged
	static constexpr uint8_t VERB_TRACE   = 5; // Step-by-step algorithm details
	static constexpr uint8_t VERB_VVVVVV  = 6; // What, can't I have a little fun?
	// clang-format on
	[[gnu::format(printf, 3, 4)]]
	void verbosePrint(uint8_t level, char const *fmt, ...) const;

	mutable bool hasTransparentPixels = false;
	uint8_t maxOpaqueColors() const { return nbColorsPerPal - hasTransparentPixels; }
};

extern Options options;

// Prints the error count, and exits with failure
[[noreturn]]
void giveUp();
// If any error has been emitted thus far, calls `giveUp()`.
void requireZeroErrors();
// Prints a warning, and does not change the error count
[[gnu::format(printf, 1, 2)]]
void warning(char const *fmt, ...);
// Prints an error, and increments the error count
[[gnu::format(printf, 1, 2)]]
void error(char const *fmt, ...);
// Prints an error, and increments the error count
// Does not take format arguments so `format_` and `-Wformat-security` won't complain about
// calling `errorMessage(msg)`.
void errorMessage(char const *msg);
// Prints a fatal error, increments the error count, and gives up
[[gnu::format(printf, 1, 2), noreturn]]
void fatal(char const *fmt, ...);

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

// Flipping tends to happen fairly often, so take a bite out of dcache to speed it up
static constexpr auto flipTable = ([]() constexpr {
	std::array<uint16_t, 256> table{};
	for (uint16_t i = 0; i < table.size(); i++) {
		// To flip all the bits, we'll flip both nibbles, then each nibble half, etc.
		uint16_t byte = i;
		byte = (byte & 0b0000'1111) << 4 | (byte & 0b1111'0000) >> 4;
		byte = (byte & 0b0011'0011) << 2 | (byte & 0b1100'1100) >> 2;
		byte = (byte & 0b0101'0101) << 1 | (byte & 0b1010'1010) >> 1;
		table[i] = byte;
	}
	return table;
})();

// Parsing helpers.

static constexpr uint8_t nibble(char c) {
	if (c >= 'a') {
		assume(c <= 'f');
		return c - 'a' + 10;
	} else if (c >= 'A') {
		assume(c <= 'F');
		return c - 'A' + 10;
	} else {
		assume(c >= '0' && c <= '9');
		return c - '0';
	}
}

static constexpr uint8_t toHex(char c1, char c2) {
	return nibble(c1) * 16 + nibble(c2);
}

static constexpr uint8_t singleToHex(char c) {
	return toHex(c, c);
}

#endif // RGBDS_GFX_MAIN_HPP
