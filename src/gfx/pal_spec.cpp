// SPDX-License-Identifier: MIT

#include "gfx/pal_spec.hpp"

#include <algorithm>
#include <charconv>
#include <errno.h>
#include <fstream>
#include <inttypes.h>
#include <limits.h>
#include <optional>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <streambuf>
#include <string.h>
#include <string>
#include <string_view>
#include <tuple>

#include "diagnostics.hpp"
#include "helpers.hpp"
#include "platform.hpp"

#include "gfx/main.hpp"
#include "gfx/warning.hpp"

using namespace std::string_view_literals;

static char const *hexDigits = "0123456789ABCDEFabcdef";

template<typename Str> // Should be std::string or std::string_view
static void skipWhitespace(Str const &str, size_t &pos) {
	pos = std::min(str.find_first_not_of(" \t"sv, pos), str.length());
}

static constexpr uint8_t nibble(char c) {
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

static constexpr uint8_t toHex(char c1, char c2) {
	return nibble(c1) * 16 + nibble(c2);
}

static constexpr uint8_t singleToHex(char c) {
	return toHex(c, c);
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
		    "In inline palette spec: %s\n"
		    "                        ",
		    rawArg
		);
		for (size_t i = ofs; i; --i) {
			putc(' ', stderr);
		}
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

#define requireLine(kind, file, buffer) \
	do { \
		if (!readLine(file, buffer)) { \
			error(kind " palette file is shorter than expected"); \
			return; \
		} \
	} while (0)

// Parses the initial part of a string_view, advancing the "read index" as it does
template<typename U> // Should be uint*_t
static std::optional<U> parseDec(std::string const &str, size_t &n) {
	uintmax_t value = 0;
	auto result = std::from_chars(str.data() + n, str.data() + str.length(), value);
	if (static_cast<bool>(result.ec)) {
		return std::nullopt;
	}
	n = result.ptr - str.data();
	return std::optional<U>{value};
}

static std::optional<Rgba> parseColor(std::string const &str, size_t &n, uint16_t i) {
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
	size_t n = 0;
	std::optional<uint16_t> nbColors = parseDec<uint16_t>(line, n);
	if (!nbColors || n != line.length()) {
		error("Invalid \"number of colors\" line in PSP file (%s)", line.c_str());
		return;
	}

	if (uint16_t nbPalColors = options.nbColorsPerPal * options.nbPalettes;
	    *nbColors > nbPalColors) {
		warnx(
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

		size_t n = 0;
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
		warnx(
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
		warnx(
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
	size_t len = file.sgetn(buf.data(), buf.size());

	uint16_t nbColors = 256;
	if (len == 772) {
		nbColors = readBE<uint16_t>(&buf[768]);
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
		warnx(
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
		warnx(
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
}

static void parseGBCFile(std::filebuf &file) {
	// This only needs to be able to read back files generated by `rgbgfx -p`
	options.palSpec.clear();

	for (;;) {
		char buf[2 * 4];
		if (size_t len = file.sgetn(buf, sizeof(buf)); len == 0) {
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

[[noreturn]]
static void handlePngError(png_structp, char const *msg) {
	fatal("Error reading palette file: %s", msg);
}

static void handlePngWarning(png_structp, char const *msg) {
	warnx("In palette file: %s", msg);
}

static void readPngData(png_structp png, png_bytep data, size_t length) {
	std::filebuf *file = reinterpret_cast<std::filebuf *>(png_get_io_ptr(png));
	std::streamsize expectedLen = length;
	std::streamsize nbBytesRead = file->sgetn(reinterpret_cast<char *>(data), expectedLen);

	if (nbBytesRead != expectedLen) {
		fatal(
		    "Error reading palette file: file too short (expected at least %zd more bytes after "
		    "reading %zu)",
		    length - nbBytesRead,
		    static_cast<size_t>(file->pubseekoff(0, std::ios_base::cur))
		);
	}
}

static bool checkPngSwatch(std::vector<png_byte> const &image, uint32_t base, uint32_t swatchSize) {
	Rgba topLeft(image[base], image[base + 1], image[base + 2], image[base + 3]);
	uint32_t rowFactor = swatchSize * options.nbColorsPerPal;
	for (uint32_t y = 0; y < swatchSize; y++) {
		for (uint32_t x = 0; x < swatchSize; x++) {
			if (x == 0 && y == 0) {
				continue;
			}
			uint32_t offset = base + (y * rowFactor + x) * 4;
			Rgba pixel(image[offset], image[offset + 1], image[offset + 2], image[offset + 3]);
			if (pixel != topLeft) {
				return false;
			}
		}
	}
	return true;
}

static void parsePNGFile(std::filebuf &file) {
	std::array<unsigned char, 8> pngHeader;
	if (file.sgetn(reinterpret_cast<char *>(pngHeader.data()), pngHeader.size())
	        != static_cast<std::streamsize>(pngHeader.size()) // Not enough bytes?
	    || png_sig_cmp(pngHeader.data(), 0, pngHeader.size()) != 0) {
		// LCOV_EXCL_START
		error("Palette file does not appear to be a PNG palette file");
		return;
		// LCOV_EXCL_STOP
	}

	png_structp png =
	    png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, handlePngError, handlePngWarning);
	if (!png) {
		// LCOV_EXCL_START
		error("Failed to create PNG read structure: %s", strerror(errno));
		return;
		// LCOV_EXCL_STOP
	}

	png_infop info = png_create_info_struct(png);
	Defer destroyPng{[&] { png_destroy_read_struct(&png, &info, nullptr); }};
	if (!info) {
		// LCOV_EXCL_START
		error("Failed to create PNG info structure: %s", strerror(errno));
		return;
		// LCOV_EXCL_STOP
	}

	png_set_read_fn(png, &file, readPngData);
	png_set_sig_bytes(png, pngHeader.size());

	// Process all chunks up to but not including the image data
	png_read_info(png, info);

	uint32_t width, height;
	int bitDepth, colorType, interlaceType;
	png_get_IHDR(
	    png, info, &width, &height, &bitDepth, &colorType, &interlaceType, nullptr, nullptr
	);

	png_colorp embeddedPal = nullptr;
	int nbColors;
	png_bytep transparencyPal = nullptr;
	int nbTransparentEntries;
	if (png_get_PLTE(png, info, &embeddedPal, &nbColors) != 0) {
		if (png_get_tRNS(png, info, &transparencyPal, &nbTransparentEntries, nullptr)) {
			assume(nbTransparentEntries <= nbColors);
		}
	}

	// Set up transformations to turn everything into RGBA888 for simplicity of handling

	// Convert grayscale to RGB
	switch (colorType & ~PNG_COLOR_MASK_ALPHA) {
	case PNG_COLOR_TYPE_GRAY:
		png_set_gray_to_rgb(png); // This also converts tRNS to alpha
		break;
	case PNG_COLOR_TYPE_PALETTE:
		png_set_palette_to_rgb(png);
		break;
	}

	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		// If we read a tRNS chunk, convert it to alpha
		png_set_tRNS_to_alpha(png);
	} else if (!(colorType & PNG_COLOR_MASK_ALPHA)) {
		// Otherwise, if we lack an alpha channel, default to full opacity
		png_set_add_alpha(png, 0xFFFF, PNG_FILLER_AFTER);
	}

	// Scale 16bpp back to 8 (we don't need all of that precision anyway)
	if (bitDepth == 16) {
		png_set_scale_16(png);
	} else if (bitDepth < 8) {
		png_set_packing(png);
	}

	if (interlaceType != PNG_INTERLACE_NONE) {
		png_set_interlace_handling(png);
	}

	// Update `info` with the transformations
	png_read_update_info(png, info);
	// These shouldn't have changed
	assume(png_get_image_width(png, info) == width);
	assume(png_get_image_height(png, info) == height);
	// These should have changed, however
	assume(png_get_color_type(png, info) == PNG_COLOR_TYPE_RGBA);
	assume(png_get_bit_depth(png, info) == 8);

	// Now that metadata has been read, we can process the image data

	std::vector<png_byte> image(width * height * 4);
	std::vector<png_bytep> rowPtrs(height);
	for (uint32_t y = 0; y < height; ++y) {
		rowPtrs[y] = image.data() + y * width * 4;
	}
	png_read_image(png, rowPtrs.data());

	// The image width must evenly divide into a color swatch for each color per palette
	if (width % options.nbColorsPerPal != 0) {
		error(
		    "PNG palette file is %" PRIu32 "x%" PRIu32 ", which is not a multiple of %" PRIu8
		    " color swatches wide",
		    width,
		    height,
		    options.nbColorsPerPal
		);
		return;
	}

	// Infer the color swatch size (width and height) from the image width
	uint32_t swatchSize = width / options.nbColorsPerPal;

	// The image height must evenly divide into a color swatch for each palette
	if (height % swatchSize != 0) {
		error(
		    "PNG palette file is %" PRIu32 "x%" PRIu32 ", which is not a multiple of %" PRIu32
		    " pixels high",
		    width,
		    height,
		    swatchSize
		);
		return;
	}

	// More palettes than the maximum are a warning, not an error
	uint32_t nbPals = height / swatchSize;
	if (nbPals > options.nbPalettes) {
		warnx(
		    "PNG palette file contains %" PRIu32 " palette rows, but there can only be %" PRIu16
		    "; ignoring extra",
		    nbPals,
		    options.nbPalettes
		);
		nbPals = options.nbPalettes;
	}

	options.palSpec.clear();

	// Get each color from the top-left pixel of each swatch
	uint32_t colorFactor = swatchSize * 4;
	uint32_t palFactor = swatchSize * options.nbColorsPerPal;
	for (uint32_t palIdx = 0; palIdx < nbPals; ++palIdx) {
		options.palSpec.emplace_back();
		for (uint32_t colorIdx = 0; colorIdx < options.nbColorsPerPal; ++colorIdx) {
			std::optional<Rgba> &color = options.palSpec.back()[colorIdx];
			uint32_t offset = (palIdx * palFactor + colorIdx) * colorFactor;
			color = Rgba(image[offset], image[offset + 1], image[offset + 2], image[offset + 3]);

			// Check that each swatch is completely one color
			if (!checkPngSwatch(image, offset, swatchSize)) {
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
	    std::tuple{"PNG", &parsePNGFile, std::ios::binary},
	};

	auto iter = std::find_if(RANGE(parsers), [&arg, &ptr](auto const &parser) {
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
		fatal("Failed to open palette file \"%s\": %s", path, strerror(errno));
		return;
	}

	std::get<1> (*iter)(file);
}

void parseDmgPalSpec(char const * const rawArg) {
	// Two hex digit DMG palette spec

	std::string_view arg(rawArg);

	if (arg.length() != 2 || arg.find_first_not_of(hexDigits) != std::string_view::npos) {
		error("Unknown DMG palette specification \"%s\"", rawArg);
		return;
	}

	options.palSpecDmg = toHex(arg[0], arg[1]);

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
		error("Background color specification must be `#rgb`, `#rrggbb`, or `transparent`");
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
