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

#define XFLIP 0x40
#define YFLIP 0x20

void raw_to_gb(const struct RawIndexedImage *raw_image, struct GBImage *gb);
void output_file(const struct Options *opts, const struct GBImage *gb);
int get_tile_index(uint8_t *tile, uint8_t **tiles, int num_tiles,
		   int tile_size);
uint8_t reverse_bits(uint8_t b);
void xflip(uint8_t *tile, uint8_t *tile_xflip, int tile_size);
void yflip(uint8_t *tile, uint8_t *tile_yflip, int tile_size);
int get_mirrored_tile_index(uint8_t *tile, uint8_t **tiles, int num_tiles,
			    int tile_size, int *flags);
void create_mapfiles(const struct Options *opts, struct GBImage *gb,
		     struct Mapfile *tilemap, struct Mapfile *attrmap);
void output_tilemap_file(const struct Options *opts,
			 const struct Mapfile *tilemap);
void output_attrmap_file(const struct Options *opts,
			 const struct Mapfile *attrmap);
void output_palette_file(const struct Options *opts,
			 const struct RawIndexedImage *raw_image);

#endif
