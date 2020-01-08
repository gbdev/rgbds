/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <png.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/main.h"

#include "extern/getopt.h"
#include "version.h"

/* Short options */
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
	{ "output-attr-map", no_argument,       NULL, 'A' },
	{ "attr-map",        required_argument, NULL, 'a' },
	{ "color-curve",     no_argument,       NULL, 'C' },
	{ "debug",           no_argument,       NULL, 'D' },
	{ "depth",           required_argument, NULL, 'd' },
	{ "fix",             no_argument,       NULL, 'f' },
	{ "fix-and-save",    no_argument,       NULL, 'F' },
	{ "horizontal",      no_argument,       NULL, 'h' },
	{ "mirror-tiles",    no_argument,       NULL, 'm' },
	{ "output",          required_argument, NULL, 'o' },
	{ "output-palette",  no_argument,       NULL, 'P' },
	{ "palette",         required_argument, NULL, 'p' },
	{ "output-tilemap",  no_argument,       NULL, 'T' },
	{ "tilemap",         required_argument, NULL, 't' },
	{ "unique-tiles",    no_argument,       NULL, 'u' },
	{ "version",         no_argument,       NULL, 'V' },
	{ "verbose",         no_argument,       NULL, 'v' },
	{ "trim-end",        required_argument, NULL, 'x' },
	{ NULL,              no_argument,       NULL, 0   }
};

static void print_usage(void)
{
	fputs(
"Usage: rgbgfx [-CDhmuVv] [-f | -F] [-a <attr_map> | -A] [-d <depth>]\n"
"              [-o <out_file>] [-p <pal_file> | -P] [-t <tile_map> | -T]\n"
"              [-x <tiles>] <file>\n"
"Useful options:\n"
"    -f, --fix                 make the input image an indexed PNG\n"
"    -m, --mirror-tiles        optimize out mirrored tiles\n"
"    -o, --output <path>       set the output binary file\n"
"    -t, --tilemap <path>      set the output tilemap file\n"
"    -u, --unique-tiles        optimize out identical tiles\n"
"    -V, --version             print RGBGFX version and exit\n"
"\n"
"For help, use `man rgbgfx' or go to https://rednex.github.io/rgbds/\n",
	      stderr);
	exit(1);
}

int main(int argc, char *argv[])
{
	int ch, size;
	struct Options opts = {0};
	struct ImageOptions png_options = {0};
	struct RawIndexedImage *raw_image;
	struct GBImage gb = {0};
	struct Mapfile tilemap = {0};
	struct Mapfile attrmap = {0};
	char *ext;

	opts.tilemapfile = "";
	opts.attrmapfile = "";
	opts.palfile = "";
	opts.outfile = "";

	depth = 2;

	while ((ch = musl_getopt_long_only(argc, argv, optstring, longopts,
					   NULL)) != -1) {
		switch (ch) {
		case 'A':
			opts.attrmapout = true;
			break;
		case 'a':
			opts.attrmapfile = optarg;
			break;
		case 'C':
			opts.colorcurve = true;
			break;
		case 'D':
			opts.debug = true;
			break;
		case 'd':
			depth = strtoul(optarg, NULL, 0);
			break;
		case 'F':
			opts.hardfix = true;
			/* fallthrough */
		case 'f':
			opts.fix = true;
			break;
		case 'h':
			opts.horizontal = true;
			break;
		case 'm':
			opts.mirror = true;
			opts.unique = true;
			break;
		case 'o':
			opts.outfile = optarg;
			break;
		case 'P':
			opts.palout = true;
			break;
		case 'p':
			opts.palfile = optarg;
			break;
		case 'T':
			opts.tilemapout = true;
			break;
		case 't':
			opts.tilemapfile = optarg;
			break;
		case 'u':
			opts.unique = true;
			break;
		case 'V':
			printf("rgbgfx %s\n", get_package_version_string());
			exit(0);
		case 'v':
			opts.verbose = true;
			break;
		case 'x':
			opts.trim = strtoul(optarg, NULL, 0);
			break;
		default:
			print_usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fputs("FATAL: no input files\n", stderr);
		print_usage();
	}

#define WARN_MISMATCH(property) \
	warnx("The PNG's " property \
	      " setting doesn't match the one defined on the command line")

	opts.infile = argv[argc - 1];

	if (depth != 1 && depth != 2)
		errx(1, "Depth option must be either 1 or 2.");

	colors = 1 << depth;

	raw_image = input_png_file(&opts, &png_options);

	png_options.tilemapfile = "";
	png_options.attrmapfile = "";
	png_options.palfile = "";

	if (png_options.horizontal != opts.horizontal) {
		if (opts.verbose)
			WARN_MISMATCH("horizontal");

		if (opts.hardfix)
			png_options.horizontal = opts.horizontal;
	}

	if (png_options.horizontal)
		opts.horizontal = png_options.horizontal;

	if (png_options.trim != opts.trim) {
		if (opts.verbose)
			WARN_MISMATCH("trim");

		if (opts.hardfix)
			png_options.trim = opts.trim;
	}

	if (png_options.trim)
		opts.trim = png_options.trim;

	if (raw_image->width % 8) {
		errx(1, "Input PNG file %s not sized correctly. The image's width must be a multiple of 8.",
		     opts.infile);
	}
	if (raw_image->width / 8 > 1 && raw_image->height % 8) {
		errx(1, "Input PNG file %s not sized correctly. If the image is more than 1 tile wide, its height must be a multiple of 8.",
		     opts.infile);
	}

	if (opts.trim &&
	    opts.trim > (raw_image->width / 8) * (raw_image->height / 8) - 1) {
		errx(1, "Trim (%i) for input raw_image file '%s' too large (max: %i)",
		     opts.trim, opts.infile,
		     (raw_image->width / 8) * (raw_image->height / 8) - 1);
	}

	if (strcmp(png_options.tilemapfile, opts.tilemapfile) != 0) {
		if (opts.verbose)
			WARN_MISMATCH("tilemap file");

		if (opts.hardfix)
			png_options.tilemapfile = opts.tilemapfile;
	}
	if (!*opts.tilemapfile)
		opts.tilemapfile = png_options.tilemapfile;

	if (png_options.tilemapout != opts.tilemapout) {
		if (opts.verbose)
			WARN_MISMATCH("tilemap file");

		if (opts.hardfix)
			png_options.tilemapout = opts.tilemapout;
	}
	if (png_options.tilemapout)
		opts.tilemapout = png_options.tilemapout;

	if (strcmp(png_options.attrmapfile, opts.attrmapfile) != 0) {
		if (opts.verbose)
			WARN_MISMATCH("attrmap file");

		if (opts.hardfix)
			png_options.attrmapfile = opts.attrmapfile;
	}
	if (!*opts.attrmapfile)
		opts.attrmapfile = png_options.attrmapfile;

	if (png_options.attrmapout != opts.attrmapout) {
		if (opts.verbose)
			WARN_MISMATCH("attrmap file");

		if (opts.hardfix)
			png_options.attrmapout = opts.attrmapout;
	}
	if (png_options.attrmapout)
		opts.attrmapout = png_options.attrmapout;

	if (strcmp(png_options.palfile, opts.palfile) != 0) {
		if (opts.verbose)
			WARN_MISMATCH("palette file");

		if (opts.hardfix)
			png_options.palfile = opts.palfile;
	}
	if (!*opts.palfile)
		opts.palfile = png_options.palfile;

	if (png_options.palout != opts.palout) {
		if (opts.verbose)
			WARN_MISMATCH("palette file");

		if (opts.hardfix)
			png_options.palout = opts.palout;
	}

#undef WARN_MISMATCH

	if (png_options.palout)
		opts.palout = png_options.palout;

	if (!*opts.tilemapfile && opts.tilemapout) {
		ext = strrchr(opts.infile, '.');

		if (ext != NULL) {
			size = ext - opts.infile + 9;
			opts.tilemapfile = malloc(size);
			strncpy(opts.tilemapfile, opts.infile, size);
			*strrchr(opts.tilemapfile, '.') = '\0';
			strcat(opts.tilemapfile, ".tilemap");
		} else {
			opts.tilemapfile = malloc(strlen(opts.infile) + 9);
			strcpy(opts.tilemapfile, opts.infile);
			strcat(opts.tilemapfile, ".tilemap");
		}
	}

	if (!*opts.attrmapfile && opts.attrmapout) {
		ext = strrchr(opts.infile, '.');

		if (ext != NULL) {
			size = ext - opts.infile + 9;
			opts.attrmapfile = malloc(size);
			strncpy(opts.attrmapfile, opts.infile, size);
			*strrchr(opts.attrmapfile, '.') = '\0';
			strcat(opts.attrmapfile, ".attrmap");
		} else {
			opts.attrmapfile = malloc(strlen(opts.infile) + 9);
			strcpy(opts.attrmapfile, opts.infile);
			strcat(opts.attrmapfile, ".attrmap");
		}
	}

	if (!*opts.palfile && opts.palout) {
		ext = strrchr(opts.infile, '.');

		if (ext != NULL) {
			size = ext - opts.infile + 5;
			opts.palfile = malloc(size);
			strncpy(opts.palfile, opts.infile, size);
			*strrchr(opts.palfile, '.') = '\0';
			strcat(opts.palfile, ".pal");
		} else {
			opts.palfile = malloc(strlen(opts.infile) + 5);
			strcpy(opts.palfile, opts.infile);
			strcat(opts.palfile, ".pal");
		}
	}

	gb.size = raw_image->width * raw_image->height * depth / 8;
	gb.data = calloc(gb.size, 1);
	gb.trim = opts.trim;
	gb.horizontal = opts.horizontal;

	if (*opts.outfile || *opts.tilemapfile || *opts.attrmapfile) {
		raw_to_gb(raw_image, &gb);
		create_mapfiles(&opts, &gb, &tilemap, &attrmap);
	}

	if (*opts.outfile)
		output_file(&opts, &gb);

	if (*opts.tilemapfile)
		output_tilemap_file(&opts, &tilemap);

	if (*opts.attrmapfile)
		output_attrmap_file(&opts, &attrmap);

	if (*opts.palfile)
		output_palette_file(&opts, raw_image);

	if (opts.fix || opts.debug)
		output_png_file(&opts, &png_options, raw_image);

	destroy_raw_image(&raw_image);
	free(gb.data);

	return 0;
}
