/* SPDX-License-Identifier: MIT */

// Declarations that all modules use, as well as `main` and related
#ifndef RGBDS_LINK_MAIN_H
#define RGBDS_LINK_MAIN_H

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "helpers.hpp"
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
extern uint16_t scrambleROMX;
extern uint8_t scrambleWRAMX;
extern uint8_t scrambleSRAM;
extern bool is32kMode;
extern bool beVerbose;
extern bool isWRA0Mode;
extern bool disablePadding;

struct FileStackNode {
	struct FileStackNode *parent;
	// Line at which the parent context was exited; meaningless for the root level
	uint32_t lineNo;

	enum FileStackNodeType type;
	union {
		std::string *name; // NODE_FILE, NODE_MACRO
		std::vector<uint32_t> *iters; // NODE_REPT
	};
};

// Helper macro for printing verbose-mode messages
#define verbosePrint(...)   do { \
					if (beVerbose) \
						fprintf(stderr, __VA_ARGS__); \
				} while (0)

/*
 * Dump a file stack to stderr
 * @param node The leaf node to dump the context of
 */
std::string const *dumpFileStack(struct FileStackNode const *node);

void warning(struct FileStackNode const *where, uint32_t lineNo,
	     char const *fmt, ...) format_(printf, 3, 4);

void error(struct FileStackNode const *where, uint32_t lineNo,
	   char const *fmt, ...) format_(printf, 3, 4);

[[noreturn]] void fatal(struct FileStackNode const *where, uint32_t lineNo,
		     char const *fmt, ...) format_(printf, 3, 4);

#endif // RGBDS_LINK_MAIN_H
