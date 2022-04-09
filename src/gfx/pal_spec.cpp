/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gfx/pal_spec.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>

#include "gfx/main.hpp"

using namespace std::string_view_literals;

constexpr uint8_t nibble(char c) {
	if (c >= 'a') {
		assert(c <= 'f');
		return c - 'a' + 10;
	} else if (c >= 'A') {
		assert(c <= 'F');
		return c - 'A' + 10;
	} else {
		assert(c >= '0' && c <= '9');
		return c - '0';
	}
}

constexpr uint8_t toHex(char c1, char c2) {
	return nibble(c1) * 16 + nibble(c2);
}

constexpr uint8_t singleToHex(char c) {
	return toHex(c, c);
}

void parseInlinePalSpec(char const * const rawArg) {
	// List of #rrggbb/#rgb colors, comma-separated, palettes are separated by colons

	std::string_view arg(rawArg);
	using size_type = decltype(arg)::size_type;

	auto parseError = [&rawArg, &arg](size_type ofs, size_type len, char const *fmt,
	                                  auto &&...args) {
		(void)arg; // With NDEBUG, `arg` is otherwise not used
		assert(ofs <= arg.length());
		assert(len <= arg.length());

		error(fmt, args...);
		fprintf(stderr,
		        "In inline palette spec: %s\n"
		        "                        ",
		        rawArg);
		for (auto i = ofs; i; --i) {
			putc(' ', stderr);
		}
		for (auto i = len; i; --i) {
			putc('^', stderr);
		}
		putc('\n', stderr);
	};

	auto skipWhitespace = [&arg](size_type &pos) {
		pos = std::min(arg.find_first_not_of(" \t", pos), arg.length());
	};

	options.palSpec.clear();
	options.palSpec
	    .emplace_back(); // Not default-initialized, but value-initialized, so we get zeros

	size_type n = 0; // Index into the argument
	// TODO: store max `nbColors` ever reached, and compare against palette size later
	size_t nbColors = 0; // Number of colors in the current palette
	for (;;) {
		++n; // Ignore the '#' (checked either by caller or previous loop iteration)

		Rgba &color = options.palSpec.back()[nbColors];
		auto pos = std::min(arg.find_first_not_of("0123456789ABCDEFabcdef"sv, n), arg.length());
		switch (pos - n) {
		case 3:
			color = Rgba(singleToHex(arg[n + 0]), singleToHex(arg[n + 1]), singleToHex(arg[n + 2]),
			             0xFF);
			break;
		case 6:
			color = Rgba(toHex(arg[n + 0], arg[n + 1]), toHex(arg[n + 2], arg[n + 3]),
			             toHex(arg[n + 4], arg[n + 5]), 0xFF);
			break;
		case 0:
			parseError(n - 1, 1, "Missing color after '#'");
			return;
		default:
			parseError(n, pos - n, "Unknown color specification");
			return;
		}
		n = pos;

		// Skip whitespace, if any
		skipWhitespace(n);

		// Skip comma/colon, or end
		if (n == arg.length()) {
			break;
		}
		switch (arg[n]) {
		case ',':
			++n; // Skip it

			++nbColors;

			// A trailing comma may be followed by a colon
			skipWhitespace(n);
			if (n == arg.length()) {
				break;
			} else if (arg[n] != ':') {
				if (nbColors == 4) {
					parseError(n, 1, "Each palette can only contain up to 4 colors");
					return;
				}
				break;
			}
			[[fallthrough]];

		case ':':
			++n;
			skipWhitespace(n);

			nbColors = 0; // Start a new palette
			// Avoid creating a spurious empty palette
			if (n != arg.length()) {
				options.palSpec.emplace_back();
			}
			break;

		default:
			parseError(n, 1, "Unexpected character, expected ',', ':', or end of argument");
			return;
		}

		// Check again to allow trailing a comma/colon
		if (n == arg.length()) {
			break;
		}
		if (arg[n] != '#') {
			parseError(n, 1, "Unexpected character, expected '#'");
			return;
		}
	}
}
