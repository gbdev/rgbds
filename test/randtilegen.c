/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 *
 * Originally:
 * // This program is hereby released to the public domain.
 * // ~aaaaaa123456789, released 2022-03-15
 * https://gist.github.com/aaaaaa123456789/3feccf085ab4f82d144d9a47fb1b4bdf
 *
 * This was modified to use libpng instead of libplum, as well as comments and style changes.
 */

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.h"

FILE *rngRecorder; // File to which the random bytes will be read
uint32_t randBits = 0; // Storage for bits read from the input stream but not yet used
uint8_t randCount = 0; // How many bits are currently stored in the above

static uint32_t getRandomBits(uint8_t count) {
	// Trying to read one more byte with `randCount` at least this high will drop some bits!
	// If the count is no higher than that limit, then the loop is guaranteed to exit without
	// reading more bytes.
	assert(count <= sizeof(randBits) * 8 + 1);

	// Read bytes until we have enough bits to serve the request
	while (count > randCount) {
		int data = getchar();
		if (data == EOF) {
			exit(0);
		}
		randBits |= (uint32_t)data << randCount;
		randCount += 8;
		fputc(data, rngRecorder);
	}

	uint32_t result = randBits & (((uint32_t)1 << count) - 1);
	randBits >>= count;
	randCount -= count;
	return result;
}

/**
 * Flush any remaining bits in the RNG storage
 */
static void flushRng(void) {
	randCount = 0;
	randBits = 0;
}

/**
 * Expand a 5-bit color component to 8 bits with minimal bias
 */
static uint8_t _5to8(uint8_t five) {
	return five << 3 | five >> 2;
}

struct Attribute {
	unsigned char palette;
	unsigned char nbColors;
};
#define NB_TILES 10 * 10

static void writePng(png_structp png, png_infop pngInfo, uint8_t width, uint8_t height, uint16_t palettes[][4], struct Attribute const *attributes, uint8_t tileData[][8][8]) {
	uint8_t const nbTiles = width * height;
	
	png_set_IHDR(png, pngInfo, width * 8, height * 8, 8, PNG_COLOR_TYPE_RGB_ALPHA,
	             getRandomBits(1) ? PNG_INTERLACE_NONE : PNG_INTERLACE_ADAM7,
	             PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	// While it would be nice to write the image little by little, I really don't want to handle
	// interlacing myself. (We're doing interlacing to test that RGBGFX correctly handles it.)
	uint8_t const SIZEOF_PIXEL = 4; // Each pixel is 4 bytes (RGBA @ 8 bits/component)
	uint8_t data[height * 8 * width * 8 * SIZEOF_PIXEL];
	uint8_t *rowPtrs[height * 8];
	for (uint8_t y = 0; y < height * 8; ++y) {
		rowPtrs[y] = &data[y * width * 8 * SIZEOF_PIXEL];
	}

	for (uint8_t p = 0; p < nbTiles; p++) {
		uint8_t tx = 8 * (p % width), ty = 8 * (p / width);
		for (uint8_t y = 0; y < 8; y++) {
			uint8_t * const row = rowPtrs[ty + y];
			for (uint8_t x = 0; x < 8; x++) {
				uint8_t * const pixel = &row[(tx + x) * SIZEOF_PIXEL];
				uint16_t color = palettes[attributes[p].palette][tileData[p][y][x]];
				pixel[0] = _5to8(color & 0x1F);
				pixel[1] = _5to8(color >> 5 & 0x1F);
				pixel[2] = _5to8(color >> 10 & 0x1F);
				pixel[3] = color & 0x8000 ? 0x00 : 0xFF;
			}
		}
	}
	png_set_rows(png, pngInfo, rowPtrs);
	png_write_png(png, pngInfo, PNG_TRANSFORM_IDENTITY, NULL);
}

static void generate_random_image(png_structp png, png_infop pngInfo) {
	struct Attribute attributes[NB_TILES];
	uint8_t tileData[NB_TILES][8][8];
	// These two are in tiles, not pixels, and in range [3; 10], hence `NB_TILES` above
	// Both width and height are 4-bit values, so nbTiles is 8-bit (OK!)
	uint8_t const width = getRandomBits(3) + 3, height = getRandomBits(3) + 3,
	              nbTiles = width * height;

	for (uint8_t p = 0; p < nbTiles; p++) {
		uint8_t pal;
		do {
			pal = getRandomBits(5);
		} while (pal == 0 || (pal > 29));
		attributes[p].palette = 2 * pal + getRandomBits(1);
		// Population count (nb of bits set), the simple way
		static uint8_t const popcount[] = {1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
		                                   1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4};
		attributes[p].nbColors = popcount[pal - 1];
		// Handle single-color tiles the simple way, without trying to pull more random bits
		if (attributes[p].nbColors < 2) {
			memset(tileData[p], 0, sizeof(tileData[p]));
			continue;
		}
		uint8_t index, total;
		for (index = 0, total = 0; index < p; index++) {
			if (attributes[index].nbColors == attributes[p].nbColors) {
				total++;
			}
		}
		// index == p at exit
		if (total) {
			index = getRandomBits(8);
			if (index < total) {
				total = index + 1;
				for (index = 0; total; index++) {
					if (attributes[index].nbColors == attributes[p].nbColors) {
						total--;
					}
					if (!total) {
						index--;
					}
				}
			} else {
				index = p;
			}
		}
		if (index != p) {
			unsigned rotation = getRandomBits(2);
			for (uint8_t y = 0; y < 8; y++) {
				for (uint8_t x = 0; x < 8; x++) {
					tileData[p][y][x] =
					    tileData[index][y ^ ((rotation & 2) ? 7 : 0)][x ^ ((rotation & 1) ? 7 : 0)];
				}
			}
		} else {
			switch (attributes[p].nbColors) {
			case 2: // Two-color tiles only need one random bit per pixel
				for (uint8_t y = 0; y < 8; y++)
					for (uint8_t x = 0; x < 8; x++)
						tileData[p][y][x] = getRandomBits(1);
				break;
			case 4: // 4-color tiles can use two random bits per pixel
				for (uint8_t y = 0; y < 8; y++)
					for (uint8_t x = 0; x < 8; x++)
						tileData[p][y][x] = getRandomBits(2);
				break;
			case 3: // 3-color tiles must draw two random bits, but reject them if out of range
				for (uint8_t y = 0; y < 8; y++) {
					for (uint8_t x = 0; x < 8; x++) {
						do {
							index = getRandomBits(2);
						} while (index == 3);
						tileData[p][y][x] = index;
					}
				}
				break;
			default: // 1-color tiles were handled earlier
				unreachable_();
			}
		}
	}

	uint16_t colors[10];
	for (uint8_t p = 0; p < 10; p++) {
		colors[p] = getRandomBits(15);
	}
	// Randomly make color #0 of all palettes transparent
	if (!getRandomBits(2)) {
		colors[0] |= 0x8000;
		colors[5] |= 0x8000;
	}

	uint16_t palettes[60][4];
	for (uint8_t p = 0; p < 60; p++) {
		uint16_t const *subpal = &colors[p & 1 ? 5 : 0];
		uint8_t total = 0;
		for (uint8_t index = 0; index < 5; index++) {
			if (p & (2 << index)) {
				palettes[p][total++] = subpal[index];
			}
		}
	}

	writePng(png, pngInfo, width, height, palettes, attributes, tileData);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fputs("usage: randtilegen <basename> [<basename> [...]]\n", stderr);
		return 2;
	}

	size_t maxBasenameLen = 0;
	for (int index = 1; index < argc; index++) {
		size_t length = strlen(argv[index]);
		if (length > maxBasenameLen) {
			maxBasenameLen = length;
		}
	}

	char filename[maxBasenameLen + sizeof("65535.png")];
	for (uint16_t i = 0;; i++) { // 65k images ought to be enough
		for (int index = 1; index < argc; index++) {
			int len = sprintf(filename, "%s%" PRIu16 ".rng", argv[index], i);
			rngRecorder = fopen(filename, "wb");
			if (!rngRecorder) {
				perror("RNG fopen");
				return 1;
			}

			filename[len - 3] = 'p'; // `.rng` -> `.png`
			FILE *img = fopen(filename, "wb");
			if (!img) {
				perror("PNG fopen");
				return 1;
			}
			png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			if (!png) {
				perror("png_create_write_struct");
				return 1;
			}
			png_infop pngInfo = png_create_info_struct(png);
			if (!pngInfo) {
				perror("png_create_info_struct");
				return 1;
			}
			if (setjmp(png_jmpbuf(png))) {
				fprintf(stderr, "FATAL: an error occurred while writing image \"%s\"\n", filename);
				return 1;
			}

			// Ensure that image generation starts on byte boundaries
			// (This is necessary so that all involved random bits are recorded in the `.rng` file)
			flushRng();

			png_init_io(png, img);
			generate_random_image(png, pngInfo);
			png_destroy_write_struct(&png, &pngInfo);
			fclose(img);
			fclose(rngRecorder);
		}

		if (i == UINT16_MAX) {
			break;
		}
	}
}
