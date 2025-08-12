// SPDX-License-Identifier: MIT

#include "usage.hpp"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

#include "helpers.hpp"
#include "style.hpp"

static constexpr size_t maxLineLen = 79;

// LCOV_EXCL_START

void Usage::printAndExit(int code) const {
	FILE *file = code ? stderr : stdout;

	// Print "Usage: <program name>"
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
	int optIndent = static_cast<int>(literal_strlen("    ") + padOpts);

	// Print the options
	if (!options.empty()) {
		style_Set(file, STYLE_GREEN, true);
		fputs("Useful options:\n", file);
	}
	for (auto const &[opts, description] : options) {
		fputs("    ", file);

		// Print the comma-separated options
		size_t optWidth = 0;
		for (size_t i = 0; i < opts.size(); ++i) {
			if (i > 0) {
				style_Reset(file);
				fputs(", ", file);
				optWidth += literal_strlen(", ");
			}
			style_Set(file, STYLE_CYAN, false);
			fputs(opts[i].c_str(), file);
			optWidth += opts[i].length();
		}
		if (optWidth < padOpts) {
			fprintf(file, "%*c", static_cast<int>(padOpts - optWidth), ' ');
		}

		// Print the description lines, indented to the same level
		for (size_t i = 0; i < description.size(); ++i) {
			style_Reset(file);
			if (i > 0) {
				fprintf(file, "\n%*c", optIndent, ' ');
			}
			fprintf(file, "  %s", description[i].c_str());
		}
		putc('\n', file);
	}

	// Print the link for further help information
	style_Reset(file);
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
	style_Set(stderr, STYLE_RED, true);
	fputs("FATAL: ", stderr);
	style_Reset(stderr);
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	putc('\n', stderr);

	printAndExit(1);
}

// LCOV_EXCL_STOP
