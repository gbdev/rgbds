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
	STYLE_BLACK,
	STYLE_RED,
	STYLE_GREEN,
	STYLE_YELLOW,
	STYLE_BLUE,
	STYLE_MAGENTA,
	STYLE_CYAN,
	STYLE_GRAY,
#else
	STYLE_BLACK,
	STYLE_BLUE,
	STYLE_GREEN,
	STYLE_CYAN,
	STYLE_RED,
	STYLE_MAGENTA,
	STYLE_YELLOW,
	STYLE_GRAY,
#endif
};

void style_Enable(bool enable);
void style_Set(FILE *file, StyleColor color, bool bold);
void style_Reset(FILE *file);

#endif // RGBDS_STYLE_HPP
