// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_MAIN_HPP
#define RGBDS_LINK_MAIN_HPP

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <variant>
#include <vector>

#include "linkdefs.hpp"
#include "verbosity.hpp"

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

struct FileStackNode {
	FileStackNodeType type;
	std::variant<
	    std::monostate,        // Default constructed; `.type` and `.data` must be set manually
	    std::vector<uint32_t>, // NODE_REPT
	    std::string            // NODE_FILE, NODE_MACRO
	    >
	    data;

	FileStackNode *parent;
	// Line at which the parent context was exited; meaningless for the root level
	uint32_t lineNo;

	// REPT iteration counts since last named node, in reverse depth order
	std::vector<uint32_t> &iters() { return std::get<std::vector<uint32_t>>(data); }
	std::vector<uint32_t> const &iters() const { return std::get<std::vector<uint32_t>>(data); }
	// File name for files, file::macro name for macros
	std::string &name() { return std::get<std::string>(data); }
	std::string const &name() const { return std::get<std::string>(data); }

	void printBacktrace(uint32_t curLineNo) const;
	std::vector<std::pair<std::string, uint32_t>> backtrace(uint32_t curLineNo) const;
};

#endif // RGBDS_LINK_MAIN_HPP
