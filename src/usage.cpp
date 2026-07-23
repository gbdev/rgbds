// SPDX-License-Identifier: MIT

#include "usage.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostics.hpp" // vfatalx
#include "helpers.hpp"
#include "platform.hpp"
#include "style.hpp"
#include "util.hpp" // parseWholeNumber
#include "version.hpp"

#if defined(_MSC_VER) || defined(__MINGW32__)
	#define WIN32_LEAN_AND_MEAN // Include less from `windows.h`
	#include <windows.h>
#else
	#include <sys/ioctl.h>
#endif

void Usage::printVersion(bool error) const {
	fprintf(error ? stderr : stdout, "%s %s\n", name.c_str(), get_package_version_string());
}

void Usage::printAndExit(int code) const {
	// Usage flags can be long lines, so wrap them at a maximum line length
	uint64_t maxLineLen = 0;
	// Use the conventional COLUMNS environment variable, if it is defined and nonzero
	if (char const *columnsStr = getenv("COLUMNS"); columnsStr) {
		if (std::optional<uint64_t> columns = parseWholeNumber(columnsStr, BASE_10);
		    columns && *columns > 0) {
			maxLineLen = *columns;
		} else {
			warnx("Ignoring invalid `COLUMNS` value \"%s\"", columnsStr);
		}
	}
	// Otherwise, use the console window width minus 1, if the output is to a console TTY
	if (maxLineLen == 0 && isatty(code ? STDERR_FILENO : STDOUT_FILENO)) {
		// LCOV_EXCL_START
#if defined(_MSC_VER) || defined(__MINGW32__)
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		if (csbi.srWindow.Right > csbi.srWindow.Left) {
			maxLineLen = static_cast<size_t>(csbi.srWindow.Right - csbi.srWindow.Left);
		}
#else
		struct winsize winSize;
		ioctl(STDOUT_FILENO, TIOCGWINSZ, &winSize);
		if (winSize.ws_col > 1) {
			maxLineLen = static_cast<size_t>(winSize.ws_col - 1);
		}
#endif
		// LCOV_EXCL_STOP
	}
	// Otherwise, just use the historically common 80 minus 1
	if (maxLineLen == 0) {
		maxLineLen = 79;
	}

	// Print "Usage: <program name>"
	FILE *file = code ? stderr : stdout;
	style_Set(file, STYLE_GREEN, true);
	fputs("Usage: ", file);
	style_Set(file, STYLE_CYAN, true);
	fputs(name.c_str(), file);
	size_t padFlags = literal_strlen("Usage: ") + name.length();

	// Print the flags after the program name, indented to the same level
	style_Set(file, STYLE_CYAN, false);
	size_t flagsWidth = padFlags;
	for (std::string const &flag : flags) {
		if (flagsWidth + 1 + flag.length() > maxLineLen) {
			fprintf(file, "\n%*c", static_cast<int>(padFlags), ' ');
			flagsWidth = padFlags;
		}
		fprintf(file, " %s", flag.c_str());
		flagsWidth += 1 + flag.length();
	}
	style_Reset(file);
	fputs("\n\n", file);

	// Measure the options' flags
	size_t padOpts = 0;
	for (auto const &item : options) {
		size_t pad = 0;
		for (size_t i = 0; i < item.first.size(); ++i) {
			if (i > 0) {
				pad += literal_strlen(", ");
			}
			pad += item.first[i].length();
		}
		if (pad > padOpts) {
			padOpts = pad;
		}
	}
	size_t optOffset = literal_strlen("    ") + padOpts;

	// Print the options
	if (!options.empty()) {
		style_Set(file, STYLE_GREEN, true);
		fputs("Useful options:\n", file);
		style_Reset(file);
	}
	for (auto const &[opts, description] : options) {
		fputs("    ", file);

		// Print the comma-separated options
		size_t optWidth = 0;
		for (size_t i = 0; i < opts.size(); ++i) {
			if (i > 0) {
				fputs(", ", file);
				optWidth += literal_strlen(", ");
			}
			style_Set(file, STYLE_CYAN, false);
			fputs(opts[i].c_str(), file);
			style_Reset(file);
			optWidth += opts[i].length();
		}

		// Measure the description lines
		size_t descLen = 0;
		for (std::string const &descLine : description) {
			if (descLine.length() > descLen) {
				descLen = descLine.length();
			}
		}

		// If the description lines would wrap around the console, put them on their own lines
		size_t optIndent = optOffset;
		if (optIndent + literal_strlen("  ") + descLen > maxLineLen) {
			optIndent = 6;
			fprintf(file, "\n%*c", static_cast<int>(optIndent), ' ');
		} else if (optWidth < padOpts) {
			fprintf(file, "%*c", static_cast<int>(padOpts - optWidth), ' ');
		}

		// Print the description lines, indented to the same level
		for (size_t i = 0; i < description.size(); ++i) {
			if (i > 0) {
				fprintf(file, "\n%*c", static_cast<int>(optIndent), ' ');
			}
			fprintf(file, "  %s", description[i].c_str());
		}
		putc('\n', file);
	}

	// Print the link for further help information
	fputs("\nFor more help, use \"", file);
	style_Set(file, STYLE_CYAN, true);
	fprintf(file, "man %s", name.c_str());
	style_Reset(file);
	fputs("\" or go to ", file);
	style_Set(file, STYLE_BLUE, true);
	fputs("https://rgbds.gbdev.io/docs/", file);
	style_Reset(file);
	putc('\n', file);

	exit(code);
}

void Usage::printAndExit(char const *fmt, ...) const {
	va_list args;
	va_start(args, fmt);
	vfatalx(fmt, args);
	va_end(args);

	printAndExit(1);
}
