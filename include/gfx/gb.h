/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_GFX_GB_H
#define RGBDS_GFX_GB_H

#include <stdint.h>
#include "gfx/main.h"

void raw_to_gb(const struct RawIndexedImage *raw_image, struct GBImage *gb);
void output_file(const struct Options *opts, const struct GBImage *gb);
int get_tile_index(uint8_t *tile, uint8_t **tiles, int num_tiles,
		   int tile_size);
void create_tilemap(const struct Options *opts, struct GBImage *gb,
		    struct Tilemap *tilemap);
void output_tilemap_file(const struct Options *opts,
			 const struct Tilemap *tilemap);
void output_palette_file(const struct Options *opts,
			 const struct RawIndexedImage *raw_image);

#endif
