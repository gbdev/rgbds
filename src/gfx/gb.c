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

void create_tilemap(const struct Options *opts, struct GBImage *gb,
		    struct Tilemap *tilemap)
{
	int i, j;
	int gb_i;
	int tile_size;
	int max_tiles;
	int num_tiles;
	int index;
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

	tilemap->data = calloc(max_tiles, sizeof(uint8_t));
	tilemap->size = 0;

	gb_i = 0;
	while (gb_i < gb_size) {
		tile = malloc(tile_size);
		for (i = 0; i < tile_size; i++) {
			tile[i] = gb->data[gb_i];
			gb_i++;
		}
		if (opts->unique) {
			index = get_tile_index(tile, tiles, num_tiles,
					       tile_size);
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
		tilemap->data[tilemap->size] = index;
		tilemap->size++;
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
			 const struct Tilemap *tilemap)
{
	FILE *f;

	f = fopen(opts->mapfile, "wb");
	if (!f)
		err(1, "Opening tilemap file '%s' failed", opts->mapfile);

	fwrite(tilemap->data, 1, tilemap->size, f);
	fclose(f);

	if (opts->mapout)
		free(opts->mapfile);
}

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
		color =
			raw_image->palette[i].blue  >> 3 << 10 |
			raw_image->palette[i].green >> 3 <<  5 |
			raw_image->palette[i].red   >> 3;
		cur_bytes[0] = color & 0xFF;
		cur_bytes[1] = color >> 8;
		fwrite(cur_bytes, 2, 1, f);
	}
	fclose(f);

	if (opts->palout)
		free(opts->palfile);
}
