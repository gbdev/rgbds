/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "gfx/main.h"

void transpose_tiles(struct GBImage *gb, int width)
{
	uint8_t *newdata;
	int i;
	int newbyte;

	newdata = calloc(gb->size, 1);
	for (i = 0; i < gb->size; i++) {
		newbyte = i / (8 * depth) * width * 8 * depth;
		newbyte = newbyte % gb->size
			+ 8 * depth * (newbyte / gb->size)
			+ i % (8 * depth);
		newdata[newbyte] = gb->data[i];
	}

	free(gb->data);

	gb->data = newdata;
}

void raw_to_gb(const struct RawIndexedImage *raw_image, struct GBImage *gb)
{
	int x, y, byte;
	uint8_t index;

	for (y = 0; y < raw_image->height; y++) {
		for (x = 0; x < raw_image->width; x++) {
			index = raw_image->data[y][x];
			index &= (1 << depth) - 1;

			byte = y * depth
				+ x / 8 * raw_image->height / 8 * 8 * depth;
			gb->data[byte] |= (index & 1) << (7 - x % 8);
			if (depth == 2) {
				gb->data[byte + 1] |=
					(index >> 1) << (7 - x % 8);
			}
		}
	}

	if (!gb->horizontal)
		transpose_tiles(gb, raw_image->width / 8);
}

void output_file(const struct Options *opts, const struct GBImage *gb)
{
	FILE *f;

	f = fopen(opts->outfile, "wb");
	if (!f)
		err(1, "Opening output file '%s' failed", opts->outfile);

	fwrite(gb->data, 1, gb->size - gb->trim * 8 * depth, f);

	fclose(f);
}

int get_tile_index(uint8_t *tile, uint8_t **tiles, int num_tiles, int tile_size)
{
	int i, j;

	for (i = 0; i < num_tiles; i++) {
		for (j = 0; j < tile_size; j++) {
			if (tile[j] != tiles[i][j])
				break;
		}

		if (j >= tile_size)
			return i;
	}
	return -1;
}

uint8_t reverse_bits(uint8_t b)
{
	uint8_t rev = 0;

	rev |= (b & 0x80) >> 7;
	rev |= (b & 0x40) >> 5;
	rev |= (b & 0x20) >> 3;
	rev |= (b & 0x10) >> 1;
	rev |= (b & 0x08) << 1;
	rev |= (b & 0x04) << 3;
	rev |= (b & 0x02) << 5;
	rev |= (b & 0x01) << 7;
	return rev;
}

void xflip(uint8_t *tile, uint8_t *tile_xflip, int tile_size)
{
	int i;

	for (i = 0; i < tile_size; i++)
		tile_xflip[i] = reverse_bits(tile[i]);
}

void yflip(uint8_t *tile, uint8_t *tile_yflip, int tile_size)
{
	int i;

	for (i = 0; i < tile_size; i++)
		tile_yflip[i] = tile[(tile_size - i - 1) ^ (depth - 1)];
}

/*
 * get_mirrored_tile_index looks for `tile` in tile array `tiles`, also
 * checking x-, y-, and xy-mirrored versions of `tile`. If one is found,
 * `*flags` is set according to the type of mirroring and the index of the
 * matched tile is returned. If no match is found, -1 is returned.
 */
int get_mirrored_tile_index(uint8_t *tile, uint8_t **tiles, int num_tiles,
			    int tile_size, int *flags)
{
	int index;
	uint8_t *tile_xflip;
	uint8_t *tile_yflip;

	index = get_tile_index(tile, tiles, num_tiles, tile_size);
	if (index >= 0) {
		*flags = 0;
		return index;
	}

	tile_yflip = malloc(tile_size);
	yflip(tile, tile_yflip, tile_size);
	index = get_tile_index(tile_yflip, tiles, num_tiles, tile_size);
	if (index >= 0) {
		*flags = YFLIP;
		free(tile_yflip);
		return index;
	}

	tile_xflip = malloc(tile_size);
	xflip(tile, tile_xflip, tile_size);
	index = get_tile_index(tile_xflip, tiles, num_tiles, tile_size);
	if (index >= 0) {
		*flags = XFLIP;
		free(tile_yflip);
		free(tile_xflip);
		return index;
	}

	yflip(tile_xflip, tile_yflip, tile_size);
	index = get_tile_index(tile_yflip, tiles, num_tiles, tile_size);
	if (index >= 0)
		*flags = XFLIP | YFLIP;

	free(tile_yflip);
	free(tile_xflip);
	return index;
}

void create_mapfiles(const struct Options *opts, struct GBImage *gb,
		     struct Mapfile *tilemap, struct Mapfile *attrmap)
{
	int i, j;
	int gb_i;
	int tile_size;
	int max_tiles;
	int num_tiles;
	int index;
	int flags;
	int gb_size;
	uint8_t *tile;
	uint8_t **tiles;

	tile_size = sizeof(uint8_t) * depth * 8;
	gb_size = gb->size - (gb->trim * tile_size);
	max_tiles = gb_size / tile_size;

	/* If the input image doesn't fill the last tile, increase the count. */
	if (gb_size > max_tiles * tile_size)
		max_tiles++;

	tiles = calloc(max_tiles, sizeof(uint8_t *));
	num_tiles = 0;

	if (*opts->tilemapfile) {
		tilemap->data = calloc(max_tiles, sizeof(uint8_t));
		tilemap->size = 0;
	}

	if (*opts->attrmapfile) {
		attrmap->data = calloc(max_tiles, sizeof(uint8_t));
		attrmap->size = 0;
	}

	gb_i = 0;
	while (gb_i < gb_size) {
		flags = 0;
		tile = malloc(tile_size);
		for (i = 0; i < tile_size; i++) {
			tile[i] = gb->data[gb_i];
			gb_i++;
		}
		if (opts->unique) {
			if (opts->mirror) {
				index = get_mirrored_tile_index(tile, tiles,
								num_tiles,
								tile_size,
								&flags);
			} else {
				index = get_tile_index(tile, tiles, num_tiles,
						       tile_size);
			}
			if (index < 0) {
				index = num_tiles;
				tiles[num_tiles] = tile;
				num_tiles++;
			}
		} else {
			index = num_tiles;
			tiles[num_tiles] = tile;
			num_tiles++;
		}
		if (*opts->tilemapfile) {
			tilemap->data[tilemap->size] = index;
			tilemap->size++;
		}
		if (*opts->attrmapfile) {
			attrmap->data[attrmap->size] = flags;
			attrmap->size++;
		}
	}

	if (opts->unique) {
		free(gb->data);
		gb->data = malloc(tile_size * num_tiles);
		for (i = 0; i < num_tiles; i++) {
			tile = tiles[i];
			for (j = 0; j < tile_size; j++)
				gb->data[i * tile_size + j] = tile[j];
		}
		gb->size = i * tile_size;
	}

	for (i = 0; i < num_tiles; i++)
		free(tiles[i]);

	free(tiles);
}

void output_tilemap_file(const struct Options *opts,
			 const struct Mapfile *tilemap)
{
	FILE *f;

	f = fopen(opts->tilemapfile, "wb");
	if (!f)
		err(1, "Opening tilemap file '%s' failed", opts->tilemapfile);

	fwrite(tilemap->data, 1, tilemap->size, f);
	fclose(f);

	if (opts->tilemapout)
		free(opts->tilemapfile);
}

void output_attrmap_file(const struct Options *opts,
			 const struct Mapfile *attrmap)
{
	FILE *f;

	f = fopen(opts->attrmapfile, "wb");
	if (!f)
		err(1, "Opening attrmap file '%s' failed", opts->attrmapfile);

	fwrite(attrmap->data, 1, attrmap->size, f);
	fclose(f);

	if (opts->attrmapout)
		free(opts->attrmapfile);
}

/*
 * based on the Gaussian-like curve used by SameBoy since commit
 * 65dd02cc52f531dbbd3a7e6014e99d5b24e71a4c (Oct 2017)
 * with ties resolved by comparing the difference of the squares.
 */
static int reverse_curve[] = {
	0,  0,  1,  1,  2,  2,  3,  3,  3,  3,  4,  4,  4,  4,  4,  4,
	5,  5,  5,  5,  5,  5,  6,  6,  6,  6,  6,  6,  6,  6,  7,  7,
	7,  7,  7,  7,  7,  7,  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  10, 10, 10, 10, 10, 10,
	10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13,
	13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14,
	14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17,
	17, 17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18,
	18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
	19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 21, 21, 21, 21,
	21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22,
	22, 23, 23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24,
	24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26,
	26, 27, 27, 27, 27, 27, 28, 28, 28, 28, 29, 29, 29, 30, 30, 31,
};

void output_palette_file(const struct Options *opts,
			 const struct RawIndexedImage *raw_image)
{
	FILE *f;
	int i, color;
	uint8_t cur_bytes[2];

	f = fopen(opts->palfile, "wb");
	if (!f)
		err(1, "Opening palette file '%s' failed", opts->palfile);

	for (i = 0; i < raw_image->num_colors; i++) {
		int r = raw_image->palette[i].red;
		int g = raw_image->palette[i].green;
		int b = raw_image->palette[i].blue;

		if (opts->colorcurve) {
			g = (g * 4 - b) / 3;
			if (g < 0)
				g = 0;

			r = reverse_curve[r];
			g = reverse_curve[g];
			b = reverse_curve[b];
		} else {
			r >>= 3;
			g >>= 3;
			b >>= 3;
		}

		color = b << 10 | g << 5 | r;
		cur_bytes[0] = color & 0xFF;
		cur_bytes[1] = color >> 8;
		fwrite(cur_bytes, 2, 1, f);
	}
	fclose(f);

	if (opts->palout)
		free(opts->palfile);
}
