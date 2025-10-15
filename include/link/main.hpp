// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_MAIN_HPP
#define RGBDS_LINK_MAIN_HPP

#include <optional>
#include <stdint.h>
#include <string>

struct Options {
	bool isDmgMode;                             // -d
	std::optional<std::string> mapFileName;     // -m
	bool noSymInMap;                            // -M
	std::optional<std::string> symFileName;     // -n
	std::optional<std::string> overlayFileName; // -O
	std::optional<std::string> outputFileName;  // -o
	uint8_t padValue;                           // -p
	bool hasPadValue = false;
	// Setting these three to 0 disables the functionality
	uint16_t scrambleROMX; // -S
	uint16_t scrambleWRAMX;
	uint16_t scrambleSRAM;
	bool is32kMode;      // -t
	bool isWRAM0Mode;    // -w
	bool disablePadding; // -x
};

extern Options options;

#endif // RGBDS_LINK_MAIN_HPP
