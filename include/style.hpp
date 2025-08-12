// SPDX-License-Identifier: MIT

#ifndef RGBDS_STYLE_HPP
#define RGBDS_STYLE_HPP

#include <stdio.h>

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
	#define STYLE_ANSI 0
#else
	#define STYLE_ANSI 1
#endif

enum StyleColor {
#if STYLE_ANSI
	// Values analogous to ANSI foreground and background SGR colors
	STYLE_BLACK,
	STYLE_RED,
	STYLE_GREEN,
	STYLE_YELLOW,
	STYLE_BLUE,
	STYLE_MAGENTA,
	STYLE_CYAN,
	STYLE_GRAY,
#else
	// Values analogous to `FOREGROUND_*` constants from `windows.h`
	STYLE_BLACK,
	STYLE_BLUE,    // bit 0
	STYLE_GREEN,   // bit 1
	STYLE_CYAN,    // STYLE_BLUE | STYLE_GREEN
	STYLE_RED,     // bit 2
	STYLE_MAGENTA, // STYLE_BLUE | STYLE_RED
	STYLE_YELLOW,  // STYLE_GREEN | STYLE_RED
	STYLE_GRAY,    // STYLE_BLUE | STYLE_GREEN | STYLE_RED
#endif
};

bool style_Parse(char const *arg);
void style_Set(FILE *file, StyleColor color, bool bold);
void style_Reset(FILE *file);

#endif // RGBDS_STYLE_HPP
