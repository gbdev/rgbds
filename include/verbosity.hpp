// SPDX-License-Identifier: MIT

#ifndef RGBDS_VERBOSITY_HPP
#define RGBDS_VERBOSITY_HPP

#include <stdio.h>

#include "style.hpp"

// This macro does not evaluate its arguments unless the condition is true.
#define verbosePrint(level, ...) \
	do { \
		if (checkVerbosity(level)) { \
			style_Set(stderr, STYLE_MAGENTA, false); \
			fprintf(stderr, __VA_ARGS__); \
			style_Reset(stderr); \
		} \
	} while (0)

enum Verbosity {
	VERB_NONE,   // 0. Default, no extra output
	VERB_CONFIG, // 1. Basic configuration, after parsing CLI options
	VERB_NOTICE, // 2. Before significant actions
	VERB_INFO,   // 3. Some intermediate action results
	VERB_DEBUG,  // 4. Internals useful for debugging
	VERB_TRACE,  // 5. Step-by-step algorithm details
	VERB_VVVVVV, // 6. What, can't I have a little fun?
};

void incrementVerbosity();
bool checkVerbosity(Verbosity level);

void printVVVVVVerbosity();

#endif // RGBDS_VERBOSITY_HPP
