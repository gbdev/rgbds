// SPDX-License-Identifier: MIT

// This implementation was based on https://github.com/agauniyal/rang/
// and adapted for RGBDS.

#include "style.hpp"

#include <stdlib.h> // getenv
#include <string.h>

#include "platform.hpp" // isatty

enum Tribool { TRI_NO, TRI_YES, TRI_MAYBE };

static Tribool forceStyle = []() {
	if (char const *forceColor = getenv("FORCE_COLOR");
	    forceColor && strcmp(forceColor, "") && strcmp(forceColor, "0")) {
		return TRI_YES;
	}
	if (char const *noColor = getenv("NO_COLOR");
	    noColor && strcmp(noColor, "") && strcmp(noColor, "0")) {
		return TRI_NO;
	}
	return TRI_MAYBE;
}();

static bool isTerminal(FILE *file) {
	static bool isOutTerminal = isatty(STDOUT_FILENO);
	static bool isErrTerminal = isatty(STDERR_FILENO);

	return (file == stdout && isOutTerminal) || (file == stderr && isErrTerminal);
}

static bool allowStyle(FILE *file) {
	return forceStyle == TRI_YES || (forceStyle == TRI_MAYBE && isTerminal(file));
}

void style_Enable(bool enable) {
	forceStyle = enable ? TRI_YES : TRI_NO;
}

void style_Set(FILE *file, StyleColor color, bool bold) {
	if (!allowStyle(file)) {
		return;
	}

#if STYLE_ANSI
	fprintf(file, "\033[%dm", static_cast<int>(color) + (bold ? 90 : 30));
#else
	// TODO: support Windows
#endif
}

void style_Reset(FILE *file) {
	if (!allowStyle(file)) {
		return;
	}

#if STYLE_ANSI
	fputs("\033[m", file);
#else
	// TODO: support Windows
#endif
}
