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

#include <stdlib.h>

#include "gfx/main.h"

void
transpose_tiles(struct GBImage *gb, int width)
{
	uint8_t *newdata;
	int i;
	int newbyte;

	newdata = calloc(gb->size, 1);
	for (i = 0; i < gb->size; i++) {
		newbyte = i / (8 * gb->depth) * width * 8 * gb->depth;
		newbyte = newbyte % gb->size + 8 * gb->depth * (newbyte / gb->size) + i % (8 * gb->depth);
		newdata[newbyte] = gb->data[i];
	}

	free(gb->data);

	gb->data = newdata;
}

void
png_to_gb(struct PNGImage png, struct GBImage *gb)
{
	int x, y, byte;
	png_byte *row, index;

	for (y = 0; y < png.height; y++) {
		row = png.data[y];
		for (x = 0; x < png.width; x++) {
			index = row[x / (4 * (3 - gb->depth))] >> (8 - gb->depth - (x % (4 * (3 - gb->depth)) * gb->depth)) & 3;

			if (png.type == PNG_COLOR_TYPE_GRAY) {
				index = (gb->depth == 2 ? 3 : 1) - index;
			}
			if (!gb->horizontal) {
				byte = y * gb->depth + x / 8 * png.height / 8 * 8 * gb->depth;
			} else {
				byte = y * gb->depth + x / 8 * png.height / 8 * 8 * gb->depth;
			}
			gb->data[byte] |= (index & 1) << (7 - x % 8);
			if (gb->depth > 1) {
				gb->data[byte + 1] |= (index >> 1) << (7 - x % 8);
			}
		}
	}

	if (!gb->horizontal) {
		transpose_tiles(gb, png.width / 8);
	}
}

void
output_file(struct Options opts, struct GBImage gb)
{
	FILE *f;

	f = fopen(opts.outfile, "wb");
	if (!f) {
		err(1, "Opening output file '%s' failed", opts.outfile);
	}
	fwrite(gb.data, 1, gb.size - gb.trim * 8 * gb.depth, f);

	fclose(f);
}

void
output_tilemap_file(struct Options opts)
{
	FILE *f;

	f = fopen(opts.mapfile, "wb");
	if (!f) {
		err(1, "Opening tilemap file '%s' failed", opts.mapfile);
	}
	fclose(f);

	if (opts.mapout) {
		free(opts.mapfile);
	}
}

void
output_palette_file(struct Options opts, struct PNGImage png)
{
	FILE *f;
	int i, colors, color;
	png_color *palette;

	if (png_get_PLTE(png.png, png.info, &palette, &colors)) {
		f = fopen(opts.palfile, "wb");
		if (!f) {
			err(1, "Opening palette file '%s' failed", opts.palfile);
		}
		for (i = 0; i < colors; i++) {
			color = palette[i].blue >> 3 << 10 | palette[i].green >> 3 << 5 | palette[i].red >> 3;
			fwrite(&color, 2, 1, f);
		}
		fclose(f);
	}

	if (opts.palout) {
		free(opts.palfile);
	}
}
