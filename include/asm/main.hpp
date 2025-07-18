// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_MAIN_HPP
#define RGBDS_ASM_MAIN_HPP

#include <stdio.h>
#include <string>

extern bool verbose;

#define verbosePrint(...) \
	do { \
		if (verbose) { \
			fprintf(stderr, __VA_ARGS__); \
		} \
	} while (0)

enum MissingInclude {
	INC_ERROR,    // A missing included file is an error that halts assembly
	GEN_EXIT,     // A missing included file is assumed to be generated; exit normally
	GEN_CONTINUE, // A missing included file is assumed to be generated; continue assembling
};

extern FILE *dependFile;
extern std::string targetFileName;
extern MissingInclude missingIncludeState;
extern bool generatePhonyDeps;
extern bool failedOnMissingInclude;

#endif // RGBDS_ASM_MAIN_HPP
