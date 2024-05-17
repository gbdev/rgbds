/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_LINK_MAIN_HPP
#define RGBDS_LINK_MAIN_HPP

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <variant>
#include <vector>

#include "linkdefs.hpp"

// Variables related to CLI options
extern bool isDmgMode;
extern char *linkerScriptName;
extern char const *mapFileName;
extern bool noSymInMap;
extern char const *symFileName;
extern char const *overlayFileName;
extern char const *outputFileName;
extern uint8_t padValue;
extern bool hasPadValue;
extern uint16_t scrambleROMX;
extern uint8_t scrambleWRAMX;
extern uint8_t scrambleSRAM;
extern bool is32kMode;
extern bool beVerbose;
extern bool isWRAM0Mode;
extern bool disablePadding;

// Helper macro for printing verbose-mode messages
#define verbosePrint(...) \
	do { \
		if (beVerbose) \
			fprintf(stderr, __VA_ARGS__); \
	} while (0)

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
	std::vector<uint32_t> &iters();
	std::vector<uint32_t> const &iters() const;
	// File name for files, file::macro name for macros
	std::string &name();
	std::string const &name() const;

	std::string const &dump(uint32_t curLineNo) const;
};

[[gnu::format(printf, 3, 4)]] void
    warning(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 3, 4)]] void
    error(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 3, 4), noreturn]] void
    fatal(FileStackNode const *where, uint32_t lineNo, char const *fmt, ...);

#endif // RGBDS_LINK_MAIN_HPP
