// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_WARNING_HPP
#define RGBDS_GFX_WARNING_HPP

#include "diagnostics.hpp"

enum WarningLevel {
	LEVEL_DEFAULT,    // Warnings that are enabled by default
	LEVEL_ALL,        // Warnings that probably indicate an error
	LEVEL_EVERYTHING, // Literally every warning
};

enum WarningID {
	WARNING_EMBEDDED,      // Using an embedded PNG palette without '-c embedded'
	WARNING_OBSOLETE,      // Obsolete/deprecated things
	WARNING_TRIM_NONEMPTY, // '-x' trims nonempty tiles

	NB_PLAIN_WARNINGS,

	NB_WARNINGS = NB_PLAIN_WARNINGS,
};

extern Diagnostics<WarningLevel, WarningID> warnings;

// Warns the user about problems that don't prevent valid graphics conversion
[[gnu::format(printf, 2, 3)]]
void warning(WarningID id, char const *fmt, ...);

// Prints the error count, and exits with failure
[[noreturn]]
void giveUp();

// If any error has been emitted thus far, calls `giveUp()`
void requireZeroErrors();

// Prints an error, and increments the error count
[[gnu::format(printf, 1, 2)]]
void error(char const *fmt, ...);

// Prints a fatal error, increments the error count, and gives up
[[gnu::format(printf, 1, 2), noreturn]]
void fatal(char const *fmt, ...);

#endif // RGBDS_GFX_WARNING_HPP
