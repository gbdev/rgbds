// SPDX-License-Identifier: MIT

#include "gfx/pal_spec.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <errno.h>
#include <fstream>
#include <inttypes.h>
#include <ios>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "diagnostics.hpp"
#include "helpers.hpp"
#include "platform.hpp"
#include "util.hpp" // UpperMap, isDigit

#include "gfx/main.hpp"
#include "gfx/png.hpp"
#include "gfx/rgba.hpp"
#include "gfx/warning.hpp"

using namespace std::string_view_literals;

static char const *hexDigits = "0123456789ABCDEFabcdef";

static void skipBlankSpace(std::string_view const &str, size_t &pos) {
	pos = std::min(str.find_first_not_of(" \t"sv, pos), str.length());
}

static uint8_t toHex(char c1, char c2) {
	return parseHexDigit(c1) * 16 + parseHexDigit(c2);
}

static uint8_t singleToHex(char c) {
	return toHex(c, c);
}

static uint16_t toWord(uint8_t low, uint8_t high) {
	return high << 8 | low;
}

void parseInlinePalSpec(char const * const rawArg) {
	// List of #rrggbb/#rgb colors (or #none); comma-separated.
	// Palettes are separated by colons.

	std::string_view arg(rawArg);

	auto parseError = [&rawArg, &arg](size_t ofs, size_t len, char const *msg) {
		(void)arg; // With NDEBUG, `arg` is otherwise not used
		assume(ofs <= arg.length());
		assume(len <= arg.length());

		error("%s", msg); // `format_` and `-Wformat-security` would complain about `error(msg);`
		fprintf(
		    stderr,
		    "In inline palette spec: \"%s\"\n%*c",
		    rawArg,
		    static_cast<int>(literal_strlen("In inline palette spec: \"") + ofs),
		    ' '
		);
		for (size_t i = len; i; --i) {
			putc('^', stderr);
		}
		putc('\n', stderr);
	};

	options.palSpec.clear();
	options.palSpec.emplace_back(); // Value-initialized, not default-init'd, so we get zeros

	size_t n = 0;        // Index into the argument
	size_t nbColors = 0; // Number of colors in the current palette
	for (;;) {
		++n; // Ignore the '#' (checked either by caller or previous loop iteration)

		std::optional<Rgba> &color = options.palSpec.back()[nbColors];
		// Check for "#none" first.
		if (strncasecmp(&rawArg[n], "none", literal_strlen("none")) == 0) {
			color = {};
			n += literal_strlen("none");
		} else {
			size_t pos = std::min(arg.find_first_not_of(hexDigits, n), arg.length());
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

		// Skip trailing space, if any
		skipBlankSpace(arg, n);

		// Skip comma/semicolon, or end
		if (n == arg.length()) {
			break;
		}
		switch (arg[n]) {
		case ',':
			++n; // Skip it

			++nbColors;

			// A trailing comma may be followed by a semicolon
			skipBlankSpace(arg, n);
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
			skipBlankSpace(arg, n);

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

// Appends the first line read from `file` to the end of the provided `buffer`.
// Returns true if a line was read.
[[gnu::warn_unused_result]]
static bool readLine(std::filebuf &file, std::string &buffer) {
	assume(buffer.empty());
	for (;;) {
		int c = file.sbumpc();
		if (c == std::filebuf::traits_type::eof()) {
			return !buffer.empty();
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

static void warnExtraColors(
    char const *kind, char const *filename, uint16_t nbColors, uint16_t maxNbColors
) {
	warnx(
	    "%s file \"%s\" contains %" PRIu16 " colors, but there can only be %" PRIu16
	    "; ignoring extra",
	    kind,
	    filename,
	    nbColors,
	    maxNbColors
	);
}

// Parses the initial part of a string_view, advancing the "read index" as it does
template<typename UintT> // Should be uint*_t
static std::optional<UintT> parseDec(std::string const &str, size_t &n) {
	uintmax_t value = 0;
	auto result = std::from_chars(str.data() + n, str.data() + str.length(), value);
	if (static_cast<bool>(result.ec)) {
		return std::nullopt;
	}
	n = result.ptr - str.data();
	return std::optional<UintT>{value};
}

static std::optional<Rgba> parseColor(std::string const &str, size_t &n, uint16_t i) {
	std::optional<uint8_t> r = parseDec<uint8_t>(str, n);
	if (!r) {
		error("Failed to parse color #%d (\"%s\"): invalid red component", i + 1, str.c_str());
		return std::nullopt;
	}
	skipBlankSpace(str, n);
	if (n == str.length()) {
		error("Failed to parse color #%d (\"%s\"): missing green component", i + 1, str.c_str());
		return std::nullopt;
	}
	std::optional<uint8_t> g = parseDec<uint8_t>(str, n);
	if (!g) {
		error("Failed to parse color #%d (\"%s\"): invalid green component", i + 1, str.c_str());
		return std::nullopt;
	}
	skipBlankSpace(str, n);
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

static void parsePSPFile(char const *filename, std::filebuf &file) {
	// https://www.selapa.net/swatches/colors/fileformats.php#psp_pal

#define requireLine() \
	do { \
		line.clear(); \
		if (!readLine(file, line)) { \
			error("PSP palette file \"%s\" is shorter than expected", filename); \
			return; \
		} \
	} while (0)

	std::string line;
	if (!readLine(file, line) || line != "JASC-PAL") {
		error("File \"%s\" is not a valid PSP palette file", filename);
		return;
	}

	requireLine();
	if (line != "0100") {
		error("Unsupported PSP palette file version \"%s\"", line.c_str());
		return;
	}

	requireLine();
	size_t n = 0;
	std::optional<uint16_t> nbColors = parseDec<uint16_t>(line, n);
	if (!nbColors || n != line.length()) {
		error("Invalid \"number of colors\" line in PSP file (\"%s\")", line.c_str());
		return;
	}

	if (uint16_t maxNbColors = options.maxNbColors(); *nbColors > maxNbColors) {
		warnExtraColors("PSP", filename, *nbColors, maxNbColors);
		nbColors = maxNbColors;
	}

	options.palSpec.clear();

	for (uint16_t i = 0; i < *nbColors; ++i) {
		requireLine();

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

#undef requireLine
}

static void parseGPLFile(char const *filename, std::filebuf &file) {
	// https://gitlab.gnome.org/GNOME/gimp/-/blob/gimp-2-10/app/core/gimppalette-load.c#L39

	std::string line;
	if (!readLine(file, line) || !line.starts_with("GIMP Palette")) {
		error("File \"%s\" is not a valid GPL palette file", filename);
		return;
	}

	uint16_t nbColors = 0;
	uint16_t const maxNbColors = options.maxNbColors();

	for (;;) {
		line.clear();
		if (!readLine(file, line)) {
			break;
		}
		if (line.starts_with("Name:") || line.starts_with("Columns:")) {
			continue;
		}

		size_t n = 0;
		skipBlankSpace(line, n);
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
		warnExtraColors("GPL", filename, nbColors, maxNbColors);
	}
}

static void parseHEXFile(char const *filename, std::filebuf &file) {
	// https://lospec.com/palette-list/tag/gbc

	uint16_t nbColors = 0;
	uint16_t const maxNbColors = options.maxNbColors();

	for (;;) {
		std::string line;
		if (!readLine(file, line)) {
			break;
		}
		// Ignore empty lines.
		if (line.length() == 0) {
			continue;
		}

		if (line.length() != 6 || line.find_first_not_of(hexDigits) != std::string::npos) {
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
		warnExtraColors("HEX", filename, nbColors, maxNbColors);
	}
}

static void parseACTFile(char const *filename, std::filebuf &file) {
	// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_pgfId-1070626

	std::array<char, 772> buf{};
	size_t len = file.sgetn(buf.data(), buf.size());

	uint16_t nbColors = 256;
	if (len == 772) {
		nbColors = toWord(buf[769], buf[768]);
		if (nbColors > 256 || nbColors == 0) {
			error("Invalid number of colors in ACT file \"%s\" (%" PRIu16 ")", filename, nbColors);
			return;
		}
	} else if (len != 768) {
		error(
		    "Invalid file size for ACT file \"%s\" (expected 768 or 772 bytes, got %zu)",
		    filename,
		    len
		);
		return;
	}

	if (uint16_t maxNbColors = options.maxNbColors(); nbColors > maxNbColors) {
		warnExtraColors("ACT", filename, nbColors, maxNbColors);
		nbColors = maxNbColors;
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

static void parseACOFile(char const *filename, std::filebuf &file) {
	// https://www.adobe.com/devnet-apps/photoshop/fileformatashtml/#50577411_pgfId-1055819

	char buf[10];

	if (file.sgetn(buf, 2) != 2) {
		error("Failed to read ACO file version");
		return;
	}
	if (toWord(buf[1], buf[0]) != 1) {
		error("File \"%s\" is not a valid ACO v1 file", filename);
		return;
	}

	if (file.sgetn(buf, 2) != 2) {
		error("Failed to read number of colors in palette file");
		return;
	}
	uint16_t nbColors = toWord(buf[1], buf[0]);

	if (uint16_t maxNbColors = options.maxNbColors(); nbColors > maxNbColors) {
		warnExtraColors("ACO", filename, nbColors, maxNbColors);
		nbColors = maxNbColors;
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
		uint16_t colorType = toWord(buf[1], buf[0]);
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
}

static void parseGBCFile(char const *filename, std::filebuf &file) {
	// This only needs to be able to read back files generated by `rgbgfx -p`
	options.palSpec.clear();

	for (;;) {
		char buf[2 * 4];
		if (size_t len = file.sgetn(buf, sizeof(buf)); len == 0) {
			break;
		} else if (len != sizeof(buf)) {
			error(
			    "GBC palette file \"%s\" contains %zu 8-byte palette%s, plus %zu byte%s",
			    filename,
			    options.palSpec.size(),
			    options.palSpec.size() == 1 ? "" : "s",
			    len,
			    len == 1 ? "" : "s"
			);
			break;
		}

		options.palSpec.push_back({
		    Rgba::fromCGBColor(toWord(buf[0], buf[1])),
		    Rgba::fromCGBColor(toWord(buf[2], buf[3])),
		    Rgba::fromCGBColor(toWord(buf[4], buf[5])),
		    Rgba::fromCGBColor(toWord(buf[6], buf[7])),
		});
	}
}

static bool checkPngSwatch(std::vector<Rgba> const &pixels, uint32_t base, uint32_t swatchSize) {
	for (uint32_t y = 0; y < swatchSize; ++y) {
		uint32_t yOffset = y * swatchSize * options.nbColorsPerPal + base;

		for (uint32_t x = 0; x < swatchSize; ++x) {
			if (x == 0 && y == 0) {
				continue;
			}
			if (pixels[yOffset + x] != pixels[base]) {
				return false;
			}
		}
	}
	return true;
}

static void parsePNGFile(char const *filename, std::filebuf &file) {
	Png png{filename, file};

	// The image width must evenly divide into a color swatch for each color per palette
	if (png.width % options.nbColorsPerPal != 0) {
		error(
		    "PNG palette file is %" PRIu32 "x%" PRIu32 ", which is not a multiple of %" PRIu8
		    " color swatches wide",
		    png.width,
		    png.height,
		    options.nbColorsPerPal
		);
		return;
	}

	// Infer the color swatch size (width and height) from the image width
	uint32_t swatchSize = png.width / options.nbColorsPerPal;

	// The image height must evenly divide into a color swatch for each palette
	if (png.height % swatchSize != 0) {
		error(
		    "PNG palette file is %" PRIu32 "x%" PRIu32 ", which is not a multiple of %" PRIu32
		    " pixels high",
		    png.width,
		    png.height,
		    swatchSize
		);
		return;
	}

	// More palettes than the maximum are a warning, not an error
	uint32_t nbPals = png.height / swatchSize;
	if (nbPals > options.nbPalettes) {
		warnExtraColors(
		    "PNG palette", filename, nbPals * options.nbColorsPerPal, options.maxNbColors()
		);
		nbPals = options.nbPalettes;
	}

	options.palSpec.clear();

	// Get each color from the top-left pixel of each swatch
	for (uint32_t y = 0; y < nbPals; ++y) {
		uint32_t yOffset = y * swatchSize * swatchSize * options.nbColorsPerPal;
		options.palSpec.emplace_back();

		for (uint32_t x = 0; x < options.nbColorsPerPal; ++x) {
			uint32_t offset = yOffset + x * swatchSize;
			options.palSpec.back()[x] = png.pixels[offset];

			// Check that each swatch is completely one color
			if (!checkPngSwatch(png.pixels, offset, swatchSize)) {
				error("PNG palette file uses multiple colors in one color swatch");
				return;
			}
		}
	}
}

void parseExternalPalSpec(char const *arg) {
	// `fmt:path`, parse the file according to the given format

	// Split both parts, error out if malformed
	char const *ptr = strchr(arg, ':');
	if (ptr == nullptr) {
		error("External palette spec must have format \"fmt:path\" (missing colon)");
		return;
	}
	char const *path = ptr + 1;

	static UpperMap<std::pair<void (*)(char const *, std::filebuf &), bool>> const parsers{
	    {"PSP", std::pair{&parsePSPFile, false}},
	    {"GPL", std::pair{&parseGPLFile, false}},
	    {"HEX", std::pair{&parseHEXFile, false}},
	    {"ACT", std::pair{&parseACTFile, true} },
	    {"ACO", std::pair{&parseACOFile, true} },
	    {"GBC", std::pair{&parseGBCFile, true} },
	    {"PNG", std::pair{&parsePNGFile, true} },
	};
	std::string format{arg, ptr};
	auto search = parsers.find(format);
	if (search == parsers.end()) {
		error("Unknown external palette format \"%s\"", format.c_str());
		return;
	}

	std::filebuf file;
	// Some parsers read the file in text mode, others in binary mode
	if (!file.open(path, search->second.second ? std::ios::in | std::ios::binary : std::ios::in)) {
		fatal("Failed to open palette file \"%s\": %s", path, strerror(errno));
		return;
	}

	search->second.first(path, file);
}

void parseDmgPalSpec(char const * const rawArg) {
	// Two hex digit DMG palette spec

	std::string_view arg(rawArg);

	if (arg.length() != 2 || arg.find_first_not_of(hexDigits) != std::string_view::npos) {
		error("Unknown DMG palette specification \"%s\"", rawArg);
		return;
	}

	parseDmgPalSpec(toHex(arg[0], arg[1]));
}

void parseDmgPalSpec(uint8_t palSpecDmg) {
	options.palSpecDmg = palSpecDmg;

	// Map gray shades to their DMG color indexes for fast lookup by `Rgba::grayIndex`
	for (uint8_t i = 0; i < 4; ++i) {
		options.dmgColors[options.dmgValue(i)] = i;
	}

	// Validate that DMG palette spec does not have conflicting colors
	for (uint8_t i = 0; i < 3; ++i) {
		for (uint8_t j = i + 1; j < 4; ++j) {
			if (options.dmgValue(i) == options.dmgValue(j)) {
				error("DMG palette specification maps two gray shades to the same color index");
				return;
			}
		}
	}
}

void parseBackgroundPalSpec(char const *arg) {
	if (strcasecmp(arg, "transparent") == 0) {
		options.bgColor = Rgba(0x00, 0x00, 0x00, 0x00);
		return;
	}

	if (arg[0] != '#') {
		error("Background color specification must be \"#rgb\", \"#rrggbb\", or \"transparent\"");
		return;
	}

	size_t size = strspn(&arg[1], hexDigits);
	switch (size) {
	case 3:
		options.bgColor = Rgba(singleToHex(arg[1]), singleToHex(arg[2]), singleToHex(arg[3]), 0xFF);
		break;
	case 6:
		options.bgColor =
		    Rgba(toHex(arg[1], arg[2]), toHex(arg[3], arg[4]), toHex(arg[5], arg[6]), 0xFF);
		break;
	default:
		error("Unknown background color specification \"%s\"", arg);
	}

	if (arg[size + 1] != '\0') {
		error("Unexpected text \"%s\" after background color specification", &arg[size + 1]);
	}
}
