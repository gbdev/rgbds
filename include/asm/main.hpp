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

extern FILE *dependFile;
extern std::string targetFileName;
extern bool continueAfterMissingIncludes;
extern bool generatedMissingIncludes;
extern bool failedOnMissingInclude;
extern bool generatePhonyDeps;

#endif // RGBDS_ASM_MAIN_HPP
