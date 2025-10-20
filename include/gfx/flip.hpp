// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_FLIP_HPP
#define RGBDS_GFX_FLIP_HPP

#include <array>
#include <stdint.h>

// Flipping tends to happen fairly often, so take a bite out of dcache to speed it up
static std::array<uint16_t, 256> flipTable = ([]() constexpr {
	std::array<uint16_t, 256> table{};
	for (uint16_t i = 0; i < table.size(); ++i) {
		// To flip all the bits, we'll flip both nibbles, then each nibble half, etc.
		uint16_t byte = i;
		byte = (byte & 0b0000'1111) << 4 | (byte & 0b1111'0000) >> 4;
		byte = (byte & 0b0011'0011) << 2 | (byte & 0b1100'1100) >> 2;
		byte = (byte & 0b0101'0101) << 1 | (byte & 0b1010'1010) >> 1;
		table[i] = byte;
	}
	return table;
})();

#endif // RGBDS_GFX_FLIP_HPP
