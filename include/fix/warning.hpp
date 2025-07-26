// SPDX-License-Identifier: MIT

#ifndef RGBDS_FIX_WARNING_HPP
#define RGBDS_FIX_WARNING_HPP

#include "diagnostics.hpp"

enum WarningLevel {
	LEVEL_DEFAULT,    // Warnings that are enabled by default
	LEVEL_ALL,        // Warnings that probably indicate an error
	LEVEL_EVERYTHING, // Literally every warning
};

enum WarningID {
	WARNING_MBC,        // Issues with MBC specs
	WARNING_OVERWRITE,  // Overwriting non-zero bytes
	WARNING_TRUNCATION, // Truncating values to fit

	NB_PLAIN_WARNINGS,

	NB_WARNINGS = NB_PLAIN_WARNINGS,
};

extern Diagnostics<WarningLevel, WarningID> warnings;

// Warns the user about problems that don't prevent fixing the ROM
[[gnu::format(printf, 2, 3)]]
void warning(WarningID id, char const *fmt, ...);

// Prints an error, and increments the error count
[[gnu::format(printf, 1, 2)]]
void error(char const *fmt, ...);

// Prints a fatal error and exits
[[gnu::format(printf, 1, 2)]]
void fatal(char const *fmt, ...);

void resetErrors();
uint32_t checkErrors(char const *filename);

#endif // RGBDS_FIX_WARNING_HPP
