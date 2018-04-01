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
#include <unistd.h>

#include "gfx/main.h"

#include "version.h"

static void print_usage(void)
{
	printf(
"usage: rgbgfx [-DFfhPTuVv] [-d #] [-o outfile] [-p palfile] [-t mapfile]\n"
"              [-x #] infile\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int ch, size;
	struct Options opts = {0};
	struct ImageOptions png_options = {0};
	struct RawIndexedImage *raw_image;
	struct GBImage gb = {0};
	struct Tilemap tilemap = {0};
	char *ext;
	const char *errmsg = "Warning: The PNG's %s setting is not the same as the setting defined on the command line.";

	if (argc == 1)
		print_usage();

	opts.mapfile = "";
	opts.palfile = "";
	opts.outfile = "";

	depth = 2;

	while ((ch = getopt(argc, argv, "Dd:Ffho:Tt:uPp:Vvx:")) != -1) {
		switch (ch) {
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
			opts.mapout = true;
			break;
		case 't':
			opts.mapfile = optarg;
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

	if (argc == 0)
		print_usage();

	opts.infile = argv[argc - 1];

	if (depth != 1 && depth != 2)
		errx(1, "Depth option must be either 1 or 2.");

	colors = 1 << depth;

	raw_image = input_png_file(&opts, &png_options);

	png_options.mapfile = "";
	png_options.palfile = "";

	if (png_options.horizontal != opts.horizontal) {
		if (opts.verbose)
			warnx(errmsg, "horizontal");

		if (opts.hardfix)
			png_options.horizontal = opts.horizontal;
	}

	if (png_options.horizontal)
		opts.horizontal = png_options.horizontal;

	if (png_options.trim != opts.trim) {
		if (opts.verbose)
			warnx(errmsg, "trim");

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

	if (strcmp(png_options.mapfile, opts.mapfile) != 0) {
		if (opts.verbose)
			warnx(errmsg, "tilemap file");

		if (opts.hardfix)
			png_options.mapfile = opts.mapfile;
	}
	if (!*opts.mapfile)
		opts.mapfile = png_options.mapfile;

	if (png_options.mapout != opts.mapout) {
		if (opts.verbose)
			warnx(errmsg, "tilemap file");

		if (opts.hardfix)
			png_options.mapout = opts.mapout;
	}
	if (png_options.mapout)
		opts.mapout = png_options.mapout;

	if (strcmp(png_options.palfile, opts.palfile) != 0) {
		if (opts.verbose)
			warnx(errmsg, "palette file");

		if (opts.hardfix)
			png_options.palfile = opts.palfile;
	}
	if (!*opts.palfile)
		opts.palfile = png_options.palfile;

	if (png_options.palout != opts.palout) {
		if (opts.verbose)
			warnx(errmsg, "palette file");

		if (opts.hardfix)
			png_options.palout = opts.palout;
	}

	if (png_options.palout)
		opts.palout = png_options.palout;

	if (!*opts.mapfile && opts.mapout) {
		ext = strrchr(opts.infile, '.');

		if (ext != NULL) {
			size = ext - opts.infile + 9;
			opts.mapfile = malloc(size);
			strncpy(opts.mapfile, opts.infile, size);
			*strrchr(opts.mapfile, '.') = '\0';
			strcat(opts.mapfile, ".tilemap");
		} else {
			opts.mapfile = malloc(strlen(opts.infile) + 9);
			strcpy(opts.mapfile, opts.infile);
			strcat(opts.mapfile, ".tilemap");
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

	if (*opts.outfile || *opts.mapfile) {
		raw_to_gb(raw_image, &gb);
		create_tilemap(&opts, &gb, &tilemap);
	}

	if (*opts.outfile)
		output_file(&opts, &gb);

	if (*opts.mapfile)
		output_tilemap_file(&opts, &tilemap);

	if (*opts.palfile)
		output_palette_file(&opts, raw_image);

	if (opts.fix || opts.debug)
		output_png_file(&opts, &png_options, raw_image);

	destroy_raw_image(&raw_image);
	free(gb.data);

	return 0;
}
