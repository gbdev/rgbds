// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_MAIN_HPP
#define RGBDS_LINK_MAIN_HPP

#include <stdint.h>

struct Options {
	bool isDmgMode;              // -d
	char const *mapFileName;     // -m
	bool noSymInMap;             // -M
	char const *symFileName;     // -n
	char const *overlayFileName; // -O
	char const *outputFileName;  // -o
	uint8_t padValue;            // -p
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
