/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gfx/main.hpp"

#include <algorithm>
#include <assert.h>
#include <charconv>
#include <filesystem>
#include <inttypes.h>
#include <limits>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/getopt.h"
#include "version.h"

#include "gfx/convert.hpp"

Options options;
static uintmax_t nbErrors;

void warning(char const *fmt, ...) {
	va_list ap;

	fputs("warning: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);
}

void error(char const *fmt, ...) {
	va_list ap;

	fputs("error: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max())
		nbErrors++;
}

[[noreturn]] void fatal(char const *fmt, ...) {
	va_list ap;

	fputs("FATAL: ", stderr);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	putc('\n', stderr);

	if (nbErrors != std::numeric_limits<decltype(nbErrors)>::max())
		nbErrors++;

	fprintf(stderr, "Conversion aborted after %ju error%s\n", nbErrors, nbErrors == 1 ? "" : "s");
	exit(1);
}

void Options::verbosePrint(char const *fmt, ...) const {
	if (beVerbose) {
		va_list ap;

		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

// Short options
static char const *optstring = "Aa:CDd:Ffhmo:Pp:Tt:uVvx:";

/*
 * Equivalent long options
 * Please keep in the same order as short opts
 *
 * Also, make sure long opts don't create ambiguity:
 * A long opt's name should start with the same letter as its short opt,
 * except if it doesn't create any ambiguity (`verbose` versus `version`).
 * This is because long opt matching, even to a single char, is prioritized
 * over short opt matching
 */
static struct option const longopts[] = {
	{"output-attr-map", no_argument,       NULL, 'A'},
	{"attr-map",        required_argument, NULL, 'a'},
	{"color-curve",     no_argument,       NULL, 'C'},
	{"debug",           no_argument,       NULL, 'D'},
	{"depth",           required_argument, NULL, 'd'},
	{"fix",             no_argument,       NULL, 'f'},
	{"fix-and-save",    no_argument,       NULL, 'F'},
	{"horizontal",      no_argument,       NULL, 'h'},
	{"mirror-tiles",    no_argument,       NULL, 'm'},
	{"output",          required_argument, NULL, 'o'},
	{"output-palette",  no_argument,       NULL, 'P'},
	{"palette",         required_argument, NULL, 'p'},
	{"output-tilemap",  no_argument,       NULL, 'T'},
	{"tilemap",         required_argument, NULL, 't'},
	{"unique-tiles",    no_argument,       NULL, 'u'},
	{"version",         no_argument,       NULL, 'V'},
	{"verbose",         no_argument,       NULL, 'v'},
	{"trim-end",        required_argument, NULL, 'x'},
	{NULL,              no_argument,       NULL, 0  }
};

static void print_usage(void) {
	fputs("Usage: rgbgfx [-CDhmuVv] [-f | -F] [-a <attr_map> | -A] [-d <depth>]\n"
	      "	      [-o <out_file>] [-p <pal_file> | -P] [-t <tile_map> | -T]\n"
	      "	      [-x <tiles>] <file>\n"
	      "Useful options:\n"
	      "    -f, --fix		 make the input image an indexed PNG\n"
	      "    -m, --mirror-tiles	optimize out mirrored tiles\n"
	      "    -o, --output <path>       set the output binary file\n"
	      "    -t, --tilemap <path>      set the output tilemap file\n"
	      "    -u, --unique-tiles	optimize out identical tiles\n"
	      "    -V, --version	     print RGBGFX version and exit\n"
	      "\n"
	      "For help, use `man rgbgfx' or go to https://rgbds.gbdev.io/docs/\n",
	      stderr);
	exit(1);
}

void fputsv(std::string_view const &view, FILE *f) {
	if (view.data()) {
		fwrite(view.data(), sizeof(char), view.size(), f);
	}
}

int main(int argc, char *argv[]) {
	int opt;
	bool autoAttrmap = false, autoTilemap = false, autoPalettes = false;

	auto parseDecimalArg = [&opt](auto &out) {
		char const *end = &musl_optarg[strlen(musl_optarg)];
		// `options.bitDepth` is not modified if the parse fails entirely
		auto result = std::from_chars(musl_optarg, end, out, 10);

		if (result.ec == std::errc::result_out_of_range) {
			error("Argument to option '%c' (\"%s\") is out of range", opt, musl_optarg);
			return false;
		} else if (result.ptr != end) {
			error("Invalid argument to option '%c' (\"%s\")", opt, musl_optarg);
			return false;
		}
		return true;
	};

	while ((opt = musl_getopt_long_only(argc, argv, optstring, longopts, nullptr)) != -1) {
		switch (opt) {
		case 'A':
			autoAttrmap = true;
			break;
		case 'a':
			autoAttrmap = false;
			options.attrmap = musl_optarg;
			break;
		case 'C':
			options.useColorCurve = true;
			break;
		case 'd':
			if (parseDecimalArg(options.bitDepth) && options.bitDepth != 1
			    && options.bitDepth != 2) {
				error("Bit depth must be 1 or 2, not %" PRIu8 "");
				options.bitDepth = 2;
			}
			break;
		case 'f':
			options.fixInput = true;
			break;
		case 'h':
			warning("`-h` is deprecated, use `-???` instead");
			[[fallthrough]];
		case '?': // TODO
			options.columnMajor = true;
			break;
		case 'm':
			options.allowMirroring = true;
			[[fallthrough]]; // Imply `-u`
		case 'u':
			options.allowDedup = true;
			break;
		case 'o':
			options.output = musl_optarg;
			break;
		case 'P':
			autoPalettes = true;
			break;
		case 'p':
			autoPalettes = false;
			options.palettes = musl_optarg;
			break;
		case 'T':
			autoTilemap = true;
			break;
		case 't':
			autoTilemap = false;
			options.tilemap = musl_optarg;
			break;
		case 'V':
			printf("rgbgfx %s\n", get_package_version_string());
			exit(0);
		case 'v':
			options.beVerbose = true;
			break;
		case 'x':
			parseDecimalArg(options.trim);
			break;
		case 'D':
		case 'F':
			warning("Ignoring option '%c'", musl_optopt);
			break;
		default:
			print_usage();
			exit(1);
		}
	}

	if (options.nbColorsPerPal == 0) {
		options.nbColorsPerPal = 1 << options.bitDepth;
	} else if (options.nbColorsPerPal > 1u << options.bitDepth) {
		error("%" PRIu8 "bpp palettes can only contain %u colors, not %" PRIu8, options.bitDepth,
		      1u << options.bitDepth, options.nbColorsPerPal);
	}

	if (musl_optind == argc) {
		fputs("FATAL: No input image specified\n", stderr);
		print_usage();
		exit(1);
	} else if (argc - musl_optind != 1) {
		fprintf(stderr, "FATAL: %d input images were specified instead of 1\n", argc - musl_optind);
		print_usage();
		exit(1);
	}

	options.input = argv[argc - 1];

	auto autoOutPath = [](bool autoOptEnabled, std::filesystem::path &path, char const *extension) {
		if (autoOptEnabled) {
			path = options.input;
			path.replace_extension(extension);
		}
	};
	autoOutPath(autoAttrmap, options.attrmap, ".attrmap");
	autoOutPath(autoTilemap, options.tilemap, ".tilemap");
	autoOutPath(autoPalettes, options.palettes, ".pal");

	if (options.beVerbose) {
		fprintf(stderr, "rgbgfx %s\n", get_package_version_string());
		fputs("Options:\n", stderr);
		if (options.fixInput)
			fputs("\tConvert input to indexed\n", stderr);
		if (options.columnMajor)
			fputs("\tOutput {tile,attr}map in column-major order\n", stderr);
		if (options.allowMirroring)
			fputs("\tAllow mirroring tiles\n", stderr);
		if (options.allowDedup)
			fputs("\tAllow deduplicating tiles\n", stderr);
		if (options.useColorCurve)
			fputs("\tUse color curve\n", stderr);
		fprintf(stderr, "\tBit depth: %" PRIu8 "bpp\n", options.bitDepth);
		fprintf(stderr, "\tTrim the last %" PRIu64 " tiles\n", options.trim);
		fprintf(stderr, "\tBase tile IDs: [%" PRIu8 ", %" PRIu8 "]\n", options.baseTileIDs[0],
		        options.baseTileIDs[1]);
		fprintf(stderr, "\tMax number of tiles: [%" PRIu16 ", %" PRIu16 "]\n",
		        options.maxNbTiles[0], options.maxNbTiles[1]);
		auto printPath = [](char const *name, std::filesystem::path const &path) {
			if (!path.empty()) {
				fprintf(stderr, "\t%s: %s\n", name, path.c_str());
			}
		};
		printPath("Input image", options.input);
		printPath("Output tile data", options.output);
		printPath("Output tilemap", options.tilemap);
		printPath("Output attrmap", options.attrmap);
		printPath("Output palettes", options.palettes);
		fputs("Ready.\n", stderr);
	}

	process();

	return 0;
}

void Palette::addColor(uint16_t color) {
	for (size_t i = 0; true; ++i) {
		assert(i < colors.size()); // The packing should guarantee this
		if (colors[i] == color) { // The color is already present
			break;
		} else if (colors[i] == UINT16_MAX) { // Empty slot
			colors[i] = color;
			break;
		}
	}
}

uint8_t Palette::indexOf(uint16_t color) const {
	return std::distance(colors.begin(), std::find(colors.begin(), colors.end(), color));
}

auto Palette::begin() -> decltype(colors)::iterator {
	return colors.begin();
}
auto Palette::end() -> decltype(colors)::iterator {
	return std::find(colors.begin(), colors.end(), UINT16_MAX);
}

auto Palette::begin() const -> decltype(colors)::const_iterator {
	return colors.begin();
}
auto Palette::end() const -> decltype(colors)::const_iterator {
	return std::find(colors.begin(), colors.end(), UINT16_MAX);
}
