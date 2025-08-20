// SPDX-License-Identifier: MIT

// This implementation was based on https://github.com/agauniyal/rang/
// and adapted for RGBDS.

#include "style.hpp"

#include <stdio.h>
#include <stdlib.h> // getenv
#include <string.h>

#include "platform.hpp" // isatty, strcasecmp

#if !STYLE_ANSI
// clang-format off: maintain `include` order
	#define WIN32_LEAN_AND_MEAN // Include less from `windows.h`
	#include <windows.h>
// clang-format on
#endif

enum Tribool { TRI_NO, TRI_YES, TRI_MAYBE };

#if !STYLE_ANSI
static HANDLE const outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
static HANDLE const errHandle = GetStdHandle(STD_ERROR_HANDLE);

static WORD const defaultAttrib = []() {
	if (CONSOLE_SCREEN_BUFFER_INFO info; GetConsoleScreenBufferInfo(outHandle, &info)
	                                     || GetConsoleScreenBufferInfo(errHandle, &info)) {
		return info.wAttributes;
	}
	return static_cast<WORD>((STYLE_BLACK << 4) | (STYLE_GRAY | 8));
}();

static HANDLE getHandle(FILE *file) {
	return file == stdout ? outHandle : file == stderr ? errHandle : INVALID_HANDLE_VALUE;
}
#endif // !STYLE_ANSI

static Tribool argStyle = TRI_MAYBE;

static bool isTerminal(FILE *file) {
	static bool const isOutTerminal = isatty(STDOUT_FILENO);
	static bool const isErrTerminal = isatty(STDERR_FILENO);

	return (file == stdout && isOutTerminal) || (file == stderr && isErrTerminal);
}

static bool allowStyle(FILE *file) {
	static Tribool const envStyle = []() {
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

	if (argStyle == TRI_YES) {
		return true;
	} else if (argStyle == TRI_NO) {
		return false;
	}

	if (envStyle == TRI_YES) {
		return true;
	} else if (envStyle == TRI_NO) {
		return false;
	}

	return isTerminal(file);
}

bool style_Parse(char const *arg) {
	if (!strcasecmp(arg, "always")) {
		argStyle = TRI_YES;
		return true;
	} else if (!strcasecmp(arg, "never")) {
		argStyle = TRI_NO;
		return true;
	} else if (!strcasecmp(arg, "auto")) {
		argStyle = TRI_MAYBE;
		return true;
	} else {
		return false;
	}
}

void style_Set(FILE *file, StyleColor color, bool bold) {
	if (!allowStyle(file)) {
		return;
	}

	// LCOV_EXCL_START
#if STYLE_ANSI
	fprintf(file, "\033[%dm", static_cast<int>(color) + (bold ? 90 : 30));
#else
	if (HANDLE handle = getHandle(file); handle != INVALID_HANDLE_VALUE) {
		fflush(file);
		SetConsoleTextAttribute(handle, (defaultAttrib & ~0xF) | (color | (bold ? 8 : 0)));
	}
#endif
	// LCOV_EXCL_STOP
}

void style_Reset(FILE *file) {
	if (!allowStyle(file)) {
		return;
	}

	// LCOV_EXCL_START
#if STYLE_ANSI
	fputs("\033[m", file);
#else
	if (HANDLE handle = getHandle(file); handle != INVALID_HANDLE_VALUE) {
		fflush(file);
		SetConsoleTextAttribute(handle, defaultAttrib);
	}
#endif
	// LCOV_EXCL_STOP
}
