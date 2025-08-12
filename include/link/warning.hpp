// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_WARNING_HPP
#define RGBDS_LINK_WARNING_HPP

#include <stdarg.h>
#include <stdint.h>

#include "diagnostics.hpp"

#define warningAt(where, ...) warning((where).src, (where).lineNo, __VA_ARGS__)
#define errorAt(where, ...)   error((where).src, (where).lineNo, __VA_ARGS__)
#define fatalAt(where, ...)   fatal((where).src, (where).lineNo, __VA_ARGS__)

#define fatalTwoAt(where1, where2, ...) \
	fatalTwo(*(where1).src, (where1).lineNo, *(where2).src, (where2).lineNo, __VA_ARGS__)

enum WarningLevel {
	LEVEL_DEFAULT,    // Warnings that are enabled by default
	LEVEL_ALL,        // Warnings that probably indicate an error
	LEVEL_EVERYTHING, // Literally every warning
};

enum WarningID {
	WARNING_ASSERT,       // Assertions
	WARNING_DIV,          // Undefined division behavior
	WARNING_OBSOLETE,     // Obsolete/deprecated things
	WARNING_SHIFT,        // Undefined `SHIFT` behavior
	WARNING_SHIFT_AMOUNT, // Strange `SHIFT` amount
	WARNING_TRUNCATION,   // Implicit truncation loses some bits

	NB_PLAIN_WARNINGS,

	NB_WARNINGS = NB_PLAIN_WARNINGS,
};

extern Diagnostics<WarningLevel, WarningID> warnings;

struct FileStackNode;

[[gnu::format(printf, 4, 5)]]
void warning(FileStackNode const *src, uint32_t lineNo, WarningID id, char const *fmt, ...);
[[gnu::format(printf, 3, 4)]]
void warning(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 1, 2)]]
void warning(char const *fmt, ...);

[[gnu::format(printf, 3, 4)]]
void error(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 1, 2)]]
void error(char const *fmt, ...);
[[gnu::format(printf, 1, 2)]]
void scriptError(char const *fmt, ...);

[[gnu::format(printf, 3, 4), noreturn]]
void fatal(FileStackNode const *src, uint32_t lineNo, char const *fmt, ...);
[[gnu::format(printf, 1, 2), noreturn]]
void fatal(char const *fmt, ...);

[[gnu::format(printf, 5, 6), noreturn]]
void fatalTwo(
    FileStackNode const &src1,
    uint32_t lineNo1,
    FileStackNode const &src2,
    uint32_t lineNo2,
    char const *fmt,
    ...
);

void requireZeroErrors();

#endif // RGBDS_LINK_WARNING_HPP
