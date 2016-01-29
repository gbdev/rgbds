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

#ifndef RGBDS_GFX_MAIN_H
#define RGBDS_GFX_MAIN_H

#include <stdbool.h>
#include <png.h>
#include "extern/err.h"

#define strequ(str1, str2) (strcmp(str1, str2) == 0)

struct Options {
	bool debug;
	bool verbose;
	bool hardfix;
	bool fix;
	bool binary;
	bool horizontal;
	int trim;
	char *mapfile;
	bool mapout;
	char *palfile;
	bool palout;
	char *outfile;
	char *infile;
};

struct PNGImage {
	png_struct *png;
	png_info *info;
	png_byte **data;
	int width;
	int height;
	png_byte depth;
	png_byte type;
	bool horizontal;
	int trim;
	char *mapfile;
	bool mapout;
	char *palfile;
	bool palout;
};

struct GBImage {
	uint8_t *data;
	int size;
	int depth;
	bool horizontal;
	int trim;
};

#include "gfx/png.h"
#include "gfx/gb.h"

#endif

