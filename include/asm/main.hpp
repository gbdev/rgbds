// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_MAIN_HPP
#define RGBDS_ASM_MAIN_HPP

#include <stdint.h>
#include <stdio.h>
#include <string>

#include "verbosity.hpp"

enum MissingInclude {
	INC_ERROR,    // A missing included file is an error that halts assembly
	GEN_EXIT,     // A missing included file is assumed to be generated; exit normally
	GEN_CONTINUE, // A missing included file is assumed to be generated; continue assembling
};

struct Options {
	bool exportAll = false;                         // -E
	uint8_t fixPrecision = 16;                      // -Q
	size_t maxRecursionDepth = 64;                  // -r
	char binDigits[2] = {'0', '1'};                 // -b
	char gfxDigits[4] = {'0', '1', '2', '3'};       // -g
	FILE *dependFile = nullptr;                     // -M
	std::string targetFileName;                     // -MQ, -MT
	MissingInclude missingIncludeState = INC_ERROR; // -MC, -MG
	bool generatePhonyDeps = false;                 // -MP
	std::string objectFileName;                     // -o
	uint8_t padByte = 0;                            // -p
	uint64_t maxErrors = 0;                         // -X

	~Options() {
		if (dependFile) {
			fclose(dependFile);
		}
	}

	void printDep(std::string const &depName) {
		if (dependFile) {
			fprintf(dependFile, "%s: %s\n", targetFileName.c_str(), depName.c_str());
		}
	}
};

extern Options options;

#endif // RGBDS_ASM_MAIN_HPP
