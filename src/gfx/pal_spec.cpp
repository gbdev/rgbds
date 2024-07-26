/* SPDX-License-Identifier: MIT */

#include "gfx/pal_spec.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <inttypes.h>
#include <limits.h>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <streambuf>
#include <string.h>
#include <string>
#include <string_view>
#include <tuple>

#include "helpers.hpp"
#include "platform.hpp"

#include "gfx/main.hpp"

using namespace std::string_view_literals;

constexpr uint8_t nibble(char c) {
	if (c >= 'a') {
		assume(c <= 'f');
		return c - 'a' + 10;
	} else if (c >= 'A') {
		assume(c <= 'F');
		return c - 'A' + 10;
	} else {
		assume(c >= '0' && c <= '9');
		return c - '0';
	}
}

constexpr uint8_t toHex(char c1, char c2) {
	return nibble(c1) * 16 + nibble(c2);
}

constexpr uint8_t singleToHex(char c) {
	return toHex(c, c);
}

template<typename Str> // Should be std::string or std::string_view
static void skipWhitespace(Str const &str, typename Str::size_type &pos) {
	pos = std::min(str.find_first_not_of(" \t"sv, pos), str.length());
}

void parseInlinePalSpec(char const * const rawArg) {
	// List of #rrggbb/#rgb colors (or #none); comma-separated.
	// Palettes are separated by colons.

	std::string_view arg(rawArg);
	using size_type = decltype(arg)::size_type;

	auto parseError = [&rawArg, &arg](size_type ofs, size_type len, char const *msg) {
		(void)arg; // With NDEBUG, `arg` is otherwise not used
		assume(ofs <= arg.length());
		assume(len <= arg.length());

		errorMessage(msg);
		fprintf(
		    stderr,
		    "In inline palette spec: %s\n"
		    "                        ",
		    rawArg
		);
		for (auto i = ofs; i; --i) {
			putc(' ', stderr);
		}
		for (auto i = len; i; --i) {
			putc('^', stderr);
		}
		putc('\n', stderr);
	};

	options.palSpec.clear();
	options.palSpec.emplace_back(); // Value-initialized, not default-init'd, so we get zeros

	size_type n = 0; // Index into the argument
	// TODO: store max `nbColors` ever reached, and compare against palette size later
	size_t nbColors = 0; // Number of colors in the current palette
	for (;;) {
		++n; // Ignore the '#' (checked either by caller or previous loop iteration)

		std::optional<Rgba> &color = options.palSpec.back()[nbColors];
		// Check for #none first.
		if (arg.compare(n, 4, "none"sv) == 0 || arg.compare(n, 4, "NONE"sv) == 0) {
			color = {};
			n += 4;
		} else {
			auto pos = std::min(arg.find_first_not_of("0123456789ABCDEFabcdef"sv, n), arg.length());
			switch (pos - n) {
			case 3:
				color = Rgba(
				    singleToHex(arg[n + 0]), singleToHex(arg[n + 1]), singleToHex(arg[n + 2]), 0xFF
				);
				break;
			case 6:
				color = Rgba(
				    toHex(arg[n + 0], arg[n + 1]),
				    toHex(arg[n + 2], arg[n + 3]),
				    toHex(arg[n + 4], arg[n + 5]),
				    0xFF
				);
				break;
			case 0:
				parseError(n - 1, 1, "Missing color after '#'");
				return;
			default:
				parseError(n, pos - n, "Unknown color specification");
				return;
			}
			n = pos;
		}

		// Skip whitespace, if any
		skipWhitespace(arg, n);

		// Skip comma/semicolon, or end
		if (n == arg.length()) {
			break;
		}
		switch (arg[n]) {
		case ',':
			++n; // Skip it

			++nbColors;

			// A trailing comma may be followed by a semicolon
			skipWhitespace(arg, n);
			if (n == arg.length()) {
				break;
			} else if (arg[n] != ';' && arg[n] != ':') {
				if (nbColors == 4) {
					parseError(n, 1, "Each palette can only contain up to 4 colors");
					return;
				}
				break;
			}
			[[fallthrough]];

		case ':':
		case ';':
			++n;
			skipWhitespace(arg, n);

			nbColors = 0; // Start a new palette
			// Avoid creating a spurious empty palette
			if (n != arg.length()) {
				options.palSpec.emplace_back();
			}
			break;

		default:
			parseError(n, 1, "Unexpected character, expected ',', ';', or end of argument");
			return;
		}

		// Check again to allow trailing a comma/semicolon
		if (n == arg.length()) {
			break;
		}
		if (arg[n] != '#') {
			parseError(n, 1, "Unexpected character, expected '#'");
			return;
		}
	}
}

/*
 * Tries to read some magic bytes from the provided `file`.
 * Returns whether the magic was correctly read.
 */
template<size_t n>
[[gnu::warn_unused_result]] // Ignoring failure to match is a bad idea.
static bool
    readMagic(std::filebuf &file, char const *magic) {
	assume(strlen(magic) == n);

	char magicBuf[n];
	return file.sgetn(magicBuf, n) == n && memcmp(magicBuf, magic, n);
}

template<typename T, typename U>
static T readBE(U const *bytes) {
	T val = 0;
	for (size_t i = 0; i < sizeof(val); ++i) {
		val = val << 8 | static_cast<uint8_t>(bytes[i]);
	}
	return val;
}

template<typename T, typename U>
static T readLE(U const *bytes) {
	T val = 0;
	for (size_t i = 0; i < sizeof(val); ++i) {
		val |= static_cast<uint8_t>(bytes[i]) << (i * 8);
	}
	return val;
}

/*
 * **Appends** the first line read from `file` to the end of the provided `buffer`.
 *
 * @return true if a line was read.
 */
[[gnu::warn_unused_result]] // Ignoring EOF is a bad idea.
static bool
    readLine(std::filebuf &file, std::string &buffer) {
	// TODO: maybe this can be optimized to bulk reads?
	for (;;) {
		auto c = file.sbumpc();
		if (c == std::filebuf::traits_type::eof()) {
			return false;
		}
		if (c == '\n') {
			// Discard a trailing CRLF
			if (!buffer.empty() && buffer.back() == '\r') {
				buffer.pop_back();
			}
			return true;
		}

		buffer.push_back(c);
	}
}

#define requireLine(kind, file, buffer) \
	do { \
		if (!readLine(file, buffer)) { \
			error(kind " palette file is shorter than expected"); \
			return; \
		} \
	} while (0)

/*
 * Parses the initial part of a string_view, advancing the "read index" as it does
 */
template<typename U> // Should be uint*_t
static std::optional<U> parseDec(std::string const &str, std::string::size_type &n) {
	uintmax_t value = 0;
	auto result = std::from_chars(str.data() + n, str.data() + str.size(), value);
	if (static_cast<bool>(result.ec)) {
		return std::nullopt;
	}
	n = result.ptr - str.data();
	return std::optional<U>{value};
}

static std::optional<Rgba>
    parseColor(std::string const &str, std::string::size_type &n, uint16_t i) {
	std::optional<uint8_t> r = parseDec<uint8_t>(str, n);
	if (!r) {
		error("Failed to parse color #%d (\"%s\"): invalid red component", i + 1, str.c_str());
		return std::nullopt;
	}
	skipWhitespace(str, n);
	if (n == str.length()) {
		error("Failed to parse color #%d (\"%s\"): missing green component", i + 1, str.c_str());
		return std::nullopt;
	}
	std::optional<uint8_t> g = parseDec<uint8_t>(str, n);
	if (!g) {
		error("Failed to parse color #%d (\"%s\"): invalid green component", i + 1, str.c_str());
		return std::nullopt;
	}
	skipWhitespace(str, n);
	if (n == str.length()) {
		error("Failed to parse color #%d (\"%s\"): missing blue component", i + 1, str.c_str());
		return std::nullopt;
	}
	std::optional<uint8_t> b = parseDec<uint8_t>(str, n);
	if (!b) {
		error("Failed to parse color #%d (\"%s\"): invalid blue component", i + 1, str.c_str());
		return std::nullopt;
	}

	return std::optional<Rgba>{Rgba(*r, *g, *b, 0xFF)};
}

static void parsePSPFile(std::filebuf &file) {
	// https://www.selapa.net/swatches/colors/fileformats.php#psp_pal

	std::string line;
	if (!readLine(file, line) || line != "JASC-PAL") {
		error("Palette file does not appear to be a PSP palette file");
		return;
	}

	line.clear();
	requireLine("PSP", file, line);
	if (line != "0100") {
		error("Unsupported PSP palette file version \"%s\"", line.c_str());
		return;
	}

	line.clear();
	requireLine("PSP", file, line);
	std::string::size_type n = 0;
	std::optional<uint16_t> nbColors = parseDec<uint16_t>(line, n);
	if (!nbColors || n != line.length()) {
		error("Invalid \"number of colors\" line in PSP file (%s)", line.c_str());
		return;
	}

	if (uint16_t nbPalColors = options.nbColorsPerPal * options.nbPalettes;
	    *nbColors > nbPalColors) {
		warning(
		    "PSP file contains %" PRIu16 " colors, but there can only be %" PRIu16
		    "; ignoring extra",
		    *nbColors,
		    nbPalColors
		);
		nbColors = nbPalColors;
	}

	options.palSpec.clear();

	for (uint16_t i = 0; i < *nbColors; ++i) {
		line.clear();
		requireLine("PSP", file, line);

		n = 0;
		std::optional<Rgba> color = parseColor(line, n, i + 1);
		if (!color) {
			return;
		}
		if (n != line.length()) {
			error(
			    "Failed to parse color #%d (\"%s\"): trailing characters after blue component",
			    i + 1,
			    line.c_str()
			);
			return;
		}

		if (i % options.nbColorsPerPal == 0) {
			options.palSpec.emplace_back();
		}
		options.palSpec.back()[i % options.nbColorsPerPal] = *color;
	}
}

static void parseGPLFile(std::filebuf &file) {
	// https://gitlab.gnome.org/GNOME/gimp/-/blob/gimp-2-10/app/core/gimppalette-load.c#L39

	std::string line;
	if (!readLine(file, line) || !line.starts_with("GIMP Palette")) {
		error("Palette file does not appear to be a GPL palette file");
		return;
	}

	uint16_t nbColors = 0;
	uint16_t const maxNbColors = options.nbColorsPerPal * options.nbPalettes;

	for (;;) {
		line.clear();
		if (!readLine(file, line)) {
			break;
		}
		if (line.starts_with("Name:") || line.starts_with("Columns:")) {
			continue;
		}

		std::string::size_type n = 0;
		skipWhitespace(line, n);
		// Skip empty lines, or lines that contain just a comment.
		if (line.length() == n || line[n] == '#') {
			continue;
		}

		std::optional<Rgba> color = parseColor(line, n, nbColors + 1);
		if (!color) {
			return;
		}
		// Ignore anything following the three components
		// (sometimes it's a comment, sometimes it's the color in CSS hex format, sometimes there's
		// nothing...).

		if (nbColors < maxNbColors) {
			if (nbColors % options.nbColorsPerPal == 0) {
				options.palSpec.emplace_back();
			}
			options.palSpec.back()[nbColors % options.nbColorsPerPal] = *color;
		}
		++nbColors;
	}

	if (nbColors > maxNbColors) {
		warning(
		    "GPL file contains %" PRIu16 " colors, but there can only be %" PRIu16
		    "; ignoring extra",
		    nbColors,
		    maxNbColors
		);
	}
}

static void parseHEXFile(std::filebuf &file) {
	// https://lospec.com/palette-list/tag/gbc

	uint16_t nbColors = 0;
	uint16_t const maxNbColors = options.nbColorsPerPal * options.nbPalettes;

	for (;;) {
		std::string line;
		if (!readLine(file, line)) {
			break;
		}
		// Ignore empty lines.
		if (line.length() == 0) {
			continue;
		}

		if (line.length() != 6
		    || line.find_first_not_of("0123456789ABCDEFabcdef"sv) != std::string::npos) {
			error(
			    "Failed to parse color #%d (\"%s\"): invalid \"rrggbb\" line",
			    nbColors + 1,
			    line.c_str()
			);
			return;
		}

		Rgba color =
		    Rgba(toHex(line[0], line[1]), toHex(line[2], line[3]), toHex(line[4], line[5]), 0xFF);

		if (nbColors < maxNbColors) {
			if (nbColors % options.nbColorsPerPal == 0) {
				options.palSpec.emplace_back();
			}
			options.palSpec.back()[nbColors % options.nbColorsPerPal] = color;
		}
		++nbColors;
	}

	if (nbColors > maxNbColors) {
		warning(
		    "HEX file contains %" PRIu16 " colors, but there can only be %" PRIu16
		    "; ignoring extra",
		    nbColors,
		    maxNbColors
		);
	}
}

static void parseACTFile(std::filebuf &file) {
	// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_pgfId-1070626

	std::array<char, 772> buf{};
	auto len = file.sgetn(buf.data(), buf.size());

	uint16_t nbColors = 256;
	if (len == 772) {
		nbColors = readBE<uint16_t>(&buf[768]);
		// TODO: apparently there is a "transparent color index"? What?
		if (nbColors > 256 || nbColors == 0) {
			error("Invalid number of colors in ACT file (%" PRIu16 ")", nbColors);
			return;
		}
	} else if (len != 768) {
		error("Invalid file size for ACT file (expected 768 or 772 bytes, got %zu", len);
		return;
	}

	if (uint16_t nbPalColors = options.nbColorsPerPal * options.nbPalettes;
	    nbColors > nbPalColors) {
		warning(
		    "ACT file contains %" PRIu16 " colors, but there can only be %" PRIu16
		    "; ignoring extra",
		    nbColors,
		    nbPalColors
		);
		nbColors = nbPalColors;
	}

	options.palSpec.clear();
	options.palSpec.emplace_back();

	char const *ptr = buf.data();
	size_t colorIdx = 0;
	for (uint16_t i = 0; i < nbColors; ++i) {
		std::optional<Rgba> &color = options.palSpec.back()[colorIdx];
		color = Rgba(ptr[0], ptr[1], ptr[2], 0xFF);

		ptr += 3;
		++colorIdx;
		if (colorIdx == options.nbColorsPerPal) {
			options.palSpec.emplace_back();
			colorIdx = 0;
		}
	}

	// Remove the spurious empty palette if there is one
	if (colorIdx == 0) {
		options.palSpec.pop_back();
	}
}

static void parseACOFile(std::filebuf &file) {
	// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_pgfId-1055819

	char buf[10];

	if (file.sgetn(buf, 2) != 2) {
		error("Failed to read ACO file version");
		return;
	}
	if (readBE<uint16_t>(buf) != 1) {
		error("Palette file does not appear to be an ACO v1 file");
		return;
	}

	if (file.sgetn(buf, 2) != 2) {
		error("Failed to read number of colors in palette file");
		return;
	}
	uint16_t nbColors = readBE<uint16_t>(buf);

	if (uint16_t nbPalColors = options.nbColorsPerPal * options.nbPalettes;
	    nbColors > nbPalColors) {
		warning(
		    "ACO file contains %" PRIu16 " colors, but there can only be %" PRIu16
		    "; ignoring extra",
		    nbColors,
		    nbPalColors
		);
		nbColors = nbPalColors;
	}

	options.palSpec.clear();

	for (uint16_t i = 0; i < nbColors; ++i) {
		if (file.sgetn(buf, 10) != 10) {
			error("Failed to read color #%d from palette file", i + 1);
			return;
		}

		if (i % options.nbColorsPerPal == 0) {
			options.palSpec.emplace_back();
		}

		std::optional<Rgba> &color = options.palSpec.back()[i % options.nbColorsPerPal];
		uint16_t colorType = readBE<uint16_t>(buf);
		switch (colorType) {
		case 0: // RGB
			// Only keep the MSB of the (big-endian) 16-bit values.
			color = Rgba(buf[2], buf[4], buf[6], 0xFF);
			break;
		case 1: // HSB
			error("Unsupported color type (HSB) for ACO file");
			return;
		case 2: // CMYK
			error("Unsupported color type (CMYK) for ACO file");
			return;
		case 7: // Lab
			error("Unsupported color type (Lab) for ACO file");
			return;
		case 8: // Grayscale
			error("Unsupported color type (grayscale) for ACO file");
			return;
		default:
			error("Unknown color type (%" PRIu16 ") for ACO file", colorType);
			return;
		}
	}

	// TODO: maybe scan the v2 data instead (if present)
	// `codecvt` can be used to convert from UTF-16 to UTF-8
}

static void parseGBCFile(std::filebuf &file) {
	// This only needs to be able to read back files generated by `rgbgfx -p`
	options.palSpec.clear();

	for (;;) {
		char buf[2 * 4];
		auto len = file.sgetn(buf, sizeof(buf));
		if (len == 0) {
			break;
		} else if (len != sizeof(buf)) {
			error(
			    "GBC palette dump contains %zu 8-byte palette%s, plus %zu byte%s",
			    options.palSpec.size(),
			    options.palSpec.size() == 1 ? "" : "s",
			    len,
			    len == 1 ? "" : "s"
			);
			break;
		}

		options.palSpec.push_back({
		    Rgba::fromCGBColor(readLE<uint16_t>(&buf[0])),
		    Rgba::fromCGBColor(readLE<uint16_t>(&buf[2])),
		    Rgba::fromCGBColor(readLE<uint16_t>(&buf[4])),
		    Rgba::fromCGBColor(readLE<uint16_t>(&buf[6])),
		});
	}
}

void parseExternalPalSpec(char const *arg) {
	// `fmt:path`, parse the file according to the given format

	// Split both parts, error out if malformed
	char const *ptr = strchr(arg, ':');
	if (ptr == nullptr) {
		error("External palette spec must have format `fmt:path` (missing colon)");
		return;
	}
	char const *path = ptr + 1;

	static std::array parsers{
	    std::tuple{"PSP", &parsePSPFile, std::ios::in    },
	    std::tuple{"GPL", &parseGPLFile, std::ios::in    },
	    std::tuple{"HEX", &parseHEXFile, std::ios::in    },
	    std::tuple{"ACT", &parseACTFile, std::ios::binary},
	    std::tuple{"ACO", &parseACOFile, std::ios::binary},
	    std::tuple{"GBC", &parseGBCFile, std::ios::binary},
	};

	auto iter =
	    std::find_if(RANGE(parsers), [&arg, &ptr](decltype(parsers)::value_type const &parser) {
		    return strncasecmp(arg, std::get<0>(parser), ptr - arg) == 0;
	    });
	if (iter == parsers.end()) {
		error(
		    "Unknown external palette format \"%.*s\"",
		    static_cast<int>(std::min(ptr - arg, static_cast<decltype(ptr - arg)>(INT_MAX))),
		    arg
		);
		return;
	}

	std::filebuf file;
	// Some parsers read the file in text mode, others in binary mode
	if (!file.open(path, std::ios::in | std::get<2>(*iter))) {
		error("Failed to open palette file \"%s\"", path);
		return;
	}

	std::get<1> (*iter)(file);
}
