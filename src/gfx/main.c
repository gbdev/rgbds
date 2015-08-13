/*
 * Copyright © 2013 stag019 <stag019@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "gfx/main.h"

char *progname;

static void usage(void) {
	printf(
"Usage: rgbgfx [-v] [-F] [-f] [-b] [-h] [-x #] [-t mapfile] [-T] [-p palfile]\n"
"              [-P] [-o outfile] infile\n");
	exit(1);
}

int main(int argc, char *argv[]) {
	int ch, size;
	struct Options opts = {0};
	struct PNGImage png = {0};
	struct GBImage gb = {0};
	char *ext;
	const char *errmsg = "Warning: The PNG's %s setting is not the same as the setting defined on the command line.";

	if(argc == 1) {
		usage();
	}

	progname = argv[0];

	opts.mapfile = "";
	opts.palfile = "";
	opts.outfile = "";

	while((ch = getopt(argc, argv, "DvFfbhx:Tt:Pp:o:")) != -1) {
		switch(ch) {
		case 'D':
			opts.debug = true;
			break;
		case 'v':
			opts.verbose = true;
			break;
		case 'F':
			opts.hardfix = true;
		case 'f':
			opts.fix = true;
			break;
		case 'b':
			opts.binary = true;
			break;
		case 'h':
			opts.horizontal = true;
			break;
		case 'x':
			opts.trim = strtoul(optarg, NULL, 0);
			break;
		case 'T':
			opts.mapout = true;
			break;
		case 't':
			opts.mapfile = optarg;
			break;
		case 'P':
			opts.palout = true;
			break;
		case 'p':
			opts.palfile = optarg;
			break;
		case 'o':
			opts.outfile = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if(argc == 0) {
		usage();
	}

	opts.infile = argv[argc - 1];

	input_png_file(opts, &png);

	png.mapfile = "";
	png.palfile = "";

	get_text(&png);

	if(png.horizontal != opts.horizontal) {
		if(opts.verbose) {
			warnx(errmsg, "horizontal");
		}
		if(opts.hardfix) {
			png.horizontal = opts.horizontal;
		}
	}
	if(png.horizontal) {
		opts.horizontal = png.horizontal;
	}

	if(png.trim != opts.trim) {
		if(opts.verbose) {
			warnx(errmsg, "trim");
		}
		if(opts.hardfix) {
			png.trim = opts.trim;
		}
	}
	if(png.trim) {
		opts.trim = png.trim;
	}
	if(opts.trim > png.width / 8 - 1) {
		errx(EXIT_FAILURE, "Trim (%i) for input png file '%s' too large (max: %i)", opts.trim, opts.infile, png.width / 8 - 1);
	}

	if(!strequ(png.mapfile, opts.mapfile)) {
		if(opts.verbose) {
			warnx(errmsg, "tilemap file");
		}
		if(opts.hardfix) {
			png.mapfile = opts.mapfile;
		}
	}
	if(!*opts.mapfile) {
		opts.mapfile = png.mapfile;
	}

	if(png.mapout != opts.mapout) {
		if(opts.verbose) {
			warnx(errmsg, "tilemap file");
		}
		if(opts.hardfix) {
			png.mapout = opts.mapout;
		}
	}
	if(png.mapout) {
		opts.mapout = png.mapout;
	}

	if(!strequ(png.palfile, opts.palfile)) {
		if(opts.verbose) {
			warnx(errmsg, "pallette file");
		}
		if(opts.hardfix) {
			png.palfile = opts.palfile;
		}
	}
	if(!*opts.palfile) {
		opts.palfile = png.palfile;
	}

	if(png.palout != opts.palout) {
		if(opts.verbose) {
			warnx(errmsg, "pallette file");
		}
		if(opts.hardfix) {
			png.palout = opts.palout;
		}
	}
	if(png.palout) {
		opts.palout = png.palout;
	}

	if(!*opts.mapfile && opts.mapout) {
		if((ext = strrchr(opts.infile, '.')) != NULL) {
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

	if(!*opts.palfile && opts.palout) {
		if((ext = strrchr(opts.infile, '.')) != NULL) {
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

	gb.depth = (opts.binary ? 1 : 2);
	gb.size = png.width * png.height / (4 * (3 - gb.depth));
	gb.data = calloc(gb.size, 1);
	gb.trim = opts.trim;
	gb.horizontal = opts.horizontal;

	if(*opts.outfile) {
		png_to_gb(png, &gb);
		output_file(opts, gb);
	}

	if(*opts.mapfile) {
		output_tilemap_file(opts);
	}

	if(*opts.palfile) {
		output_palette_file(opts, png);
	}

	if(opts.fix || opts.debug) {
		set_text(&png);
		output_png_file(opts, &png);
	}

	free_png_data(&png);
	free(gb.data);

	return EXIT_SUCCESS;
}

