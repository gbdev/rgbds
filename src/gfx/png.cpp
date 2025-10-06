// SPDX-License-Identifier: MIT

#include "gfx/png.hpp"

#include <array>
#include <errno.h>
#include <inttypes.h>
#include <ios>
#include <png.h>
#include <pngconf.h>
#include <stdint.h>
#include <stdio.h>
#include <streambuf>
#include <string.h>
#include <vector>

#include "diagnostics.hpp"
#include "helpers.hpp"
#include "style.hpp"
#include "verbosity.hpp"

#include "gfx/rgba.hpp"
#include "gfx/warning.hpp"

struct Input {
	char const *filename;
	std::streambuf &file;

	Input(char const *filename_, std::streambuf &file_) : filename(filename_), file(file_) {}
};

[[noreturn]]
static void handleError(png_structp png, char const *msg) {
	fatal(
	    "libpng error while reading PNG image (\"%s\"): %s",
	    reinterpret_cast<Input *>(png_get_error_ptr(png))->filename,
	    msg
	);
}

static void handleWarning(png_structp png, char const *msg) {
	warnx(
	    "libpng found while reading PNG image (\"%s\"): %s",
	    reinterpret_cast<Input *>(png_get_error_ptr(png))->filename,
	    msg
	);
}

static void readData(png_structp png, png_bytep data, size_t length) {
	Input &input = *reinterpret_cast<Input *>(png_get_io_ptr(png));
	std::streamsize expectedLen = length;
	std::streamsize nbBytesRead = input.file.sgetn(reinterpret_cast<char *>(data), expectedLen);

	if (nbBytesRead != expectedLen) {
		fatal(
		    "Error reading PNG image (\"%s\"): file too short (expected at least %zd more "
		    "bytes after reading %zu)",
		    input.filename,
		    length - nbBytesRead,
		    static_cast<size_t>(input.file.pubseekoff(0, std::ios_base::cur))
		);
	}
}

Png::Png(char const *filename, std::streambuf &file) {
	Input input(filename, file);

	verbosePrint(VERB_NOTICE, "Reading PNG file \"%s\"\n", input.filename);

	std::array<unsigned char, 8> pngHeader;
	if (input.file.sgetn(reinterpret_cast<char *>(pngHeader.data()), pngHeader.size())
	        != static_cast<std::streamsize>(pngHeader.size()) // Not enough bytes?
	    || png_sig_cmp(pngHeader.data(), 0, pngHeader.size()) != 0) {
		fatal("File \"%s\" is not a valid PNG image", input.filename); // LCOV_EXCL_LINE
	}

	verbosePrint(VERB_INFO, "PNG header signature is OK\n");

	png_structp png = png_create_read_struct(
	    PNG_LIBPNG_VER_STRING, static_cast<png_voidp>(&input), handleError, handleWarning
	);
	if (!png) {
		fatal("Failed to create PNG read structure: %s", strerror(errno)); // LCOV_EXCL_LINE
	}

	png_infop info = png_create_info_struct(png);
	Defer destroyPng{[&] { png_destroy_read_struct(&png, info ? &info : nullptr, nullptr); }};
	if (!info) {
		fatal("Failed to create PNG info structure: %s", strerror(errno)); // LCOV_EXCL_LINE
	}

	png_set_read_fn(png, &input, readData);
	png_set_sig_bytes(png, pngHeader.size());

	// Process all chunks up to but not including the image data
	png_read_info(png, info);

	int bitDepth, colorType, interlaceType;
	png_get_IHDR(
	    png, info, &width, &height, &bitDepth, &colorType, &interlaceType, nullptr, nullptr
	);

	pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height));

	auto colorTypeName = [](int type) {
		switch (type) {
		case PNG_COLOR_TYPE_GRAY:
			return "grayscale";
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			return "grayscale + alpha";
		case PNG_COLOR_TYPE_PALETTE:
			return "palette";
		case PNG_COLOR_TYPE_RGB:
			return "RGB";
		case PNG_COLOR_TYPE_RGB_ALPHA:
			return "RGB + alpha";
		default:
			return "unknown color type";
		}
	};
	auto interlaceTypeName = [](int type) {
		switch (type) {
		case PNG_INTERLACE_NONE:
			return "not interlaced";
		case PNG_INTERLACE_ADAM7:
			return "interlaced (Adam7)";
		default:
			return "unknown interlace type";
		}
	};
	verbosePrint(
	    VERB_INFO,
	    "PNG image: %" PRIu32 "x%" PRIu32 " pixels, %dbpp %s, %s\n",
	    width,
	    height,
	    bitDepth,
	    colorTypeName(colorType),
	    interlaceTypeName(interlaceType)
	);

	int nbColors = 0;
	png_colorp embeddedPal = nullptr;
	if (png_get_PLTE(png, info, &embeddedPal, &nbColors) != 0) {
		int nbTransparentEntries = 0;
		png_bytep transparencyPal = nullptr;
		if (png_get_tRNS(png, info, &transparencyPal, &nbTransparentEntries, nullptr)) {
			assume(nbTransparentEntries <= nbColors);
		}

		for (int i = 0; i < nbColors; ++i) {
			png_color const &color = embeddedPal[i];
			palette.emplace_back(
			    color.red,
			    color.green,
			    color.blue,
			    transparencyPal && i < nbTransparentEntries ? transparencyPal[i] : 0xFF
			);
		}

		if (checkVerbosity(VERB_INFO)) {
			style_Set(stderr, STYLE_MAGENTA, false);
			fprintf(stderr, "Embedded PNG palette has %d colors: [", nbColors);
			for (int i = 0; i < nbColors; ++i) {
				fprintf(stderr, "%s#%08x", i > 0 ? ", " : "", palette[i].toCSS());
			}
			fprintf(stderr, "]\n");
			style_Reset(stderr);
		}
	} else {
		verbosePrint(VERB_INFO, "No embedded PNG palette\n");
	}

	// Set up transformations to turn everything into RGBA8888 for simplicity of handling

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

	// Deinterlace rows so they can trivially be read in order
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

	// Now that metadata has been read, we can read the image data
	std::vector<png_byte> image(width * height * 4);
	std::vector<png_bytep> rowPtrs(height);
	for (uint32_t y = 0; y < height; ++y) {
		rowPtrs[y] = image.data() + y * width * 4;
	}
	png_read_image(png, rowPtrs.data());

	// We don't care about chunks after the image data (comments, etc.)
	png_read_end(png, nullptr);

	// Finally, process the image data from RGBA8888 bytes into `Rgba` colors
	for (uint32_t y = 0; y < height; ++y) {
		for (uint32_t x = 0; x < width; ++x) {
			uint32_t idx = y * width + x;
			uint32_t off = idx * 4;
			pixels[idx] = Rgba(image[off], image[off + 1], image[off + 2], image[off + 3]);
		}
	}
}
