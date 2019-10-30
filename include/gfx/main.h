/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_GFX_MAIN_H
#define RGBDS_GFX_MAIN_H

#include <png.h>
#include <stdbool.h>
#include <stdint.h>

#include "extern/err.h"

struct Options {
	bool debug;
	bool verbose;
	bool hardfix;
	bool fix;
	bool horizontal;
	bool mirror;
	bool unique;
	bool colorcurve;
	int trim;
	char *tilemapfile;
	bool tilemapout;
	char *attrmapfile;
	bool attrmapout;
	char *palfile;
	bool palout;
	char *outfile;
	char *infile;
};

struct RGBColor {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

struct ImageOptions {
	bool horizontal;
	int trim;
	char *tilemapfile;
	bool tilemapout;
	char *attrmapfile;
	bool attrmapout;
	char *palfile;
	bool palout;
};

struct PNGImage {
	png_struct *png;
	png_info *info;
	png_byte **data;
	int width;
	int height;
	png_byte depth;
	png_byte type;
};

struct RawIndexedImage {
	uint8_t **data;
	struct RGBColor *palette;
	int num_colors;
	unsigned int width;
	unsigned int height;
};

struct GBImage {
	uint8_t *data;
	int size;
	bool horizontal;
	int trim;
};

struct Mapfile {
	uint8_t *data;
	int size;
};

int depth, colors;

#include "gfx/makepng.h"
#include "gfx/gb.h"

#endif /* RGBDS_GFX_MAIN_H */
