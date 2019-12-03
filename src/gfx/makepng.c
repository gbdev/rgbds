/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/main.h"

static void initialize_png(struct PNGImage *img, FILE * f);
static struct RawIndexedImage *indexed_png_to_raw(struct PNGImage *img);
static struct RawIndexedImage *grayscale_png_to_raw(struct PNGImage *img);
static struct RawIndexedImage *truecolor_png_to_raw(struct PNGImage *img);
static void get_text(const struct PNGImage *img,
		     struct ImageOptions *png_options);
static void set_text(const struct PNGImage *img,
		     const struct ImageOptions *png_options);
static void free_png_data(const struct PNGImage *png);

struct RawIndexedImage *input_png_file(const struct Options *opts,
				       struct ImageOptions *png_options)
{
	struct PNGImage img;
	struct RawIndexedImage *raw_image;
	FILE *f;

	f = fopen(opts->infile, "rb");
	if (!f)
		err(1, "Opening input png file '%s' failed", opts->infile);

	initialize_png(&img, f);

	if (img.depth != depth) {
		if (opts->verbose) {
			warnx("Image bit depth is not %i (is %i).",
			      depth, img.depth);
		}
	}

	switch (img.type) {
	case PNG_COLOR_TYPE_PALETTE:
		raw_image = indexed_png_to_raw(&img); break;
	case PNG_COLOR_TYPE_GRAY:
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		raw_image = grayscale_png_to_raw(&img); break;
	case PNG_COLOR_TYPE_RGB:
	case PNG_COLOR_TYPE_RGB_ALPHA:
		raw_image = truecolor_png_to_raw(&img); break;
	default:
		/* Shouldn't happen, but might as well handle just in case. */
		errx(1, "Input PNG file is of invalid color type.");
	}

	get_text(&img, png_options);

	png_destroy_read_struct(&img.png, &img.info, NULL);
	fclose(f);
	free_png_data(&img);

	return raw_image;
}

void output_png_file(const struct Options *opts,
		     const struct ImageOptions *png_options,
		     const struct RawIndexedImage *raw_image)
{
	FILE *f;
	char *outfile;
	struct PNGImage img;
	png_color *png_palette;
	int i;

	/*
	 * TODO: Variable outfile is for debugging purposes. Eventually,
	 * opts.infile will be used directly.
	 */
	if (opts->debug) {
		outfile = malloc(strlen(opts->infile) + 5);
		strcpy(outfile, opts->infile);
		strcat(outfile, ".out");
	} else {
		outfile = opts->infile;
	}

	f = fopen(outfile, "wb");
	if (!f)
		err(1, "Opening output png file '%s' failed", outfile);

	img.png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
					  NULL, NULL, NULL);
	if (!img.png)
		errx(1, "Creating png structure failed");

	img.info = png_create_info_struct(img.png);
	if (!img.info)
		errx(1, "Creating png info structure failed");

	if (setjmp(png_jmpbuf(img.png)))
		exit(1);

	png_init_io(img.png, f);

	png_set_IHDR(img.png, img.info, raw_image->width, raw_image->height,
		     8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_palette = malloc(sizeof(png_color *) * raw_image->num_colors);
	for (i = 0; i < raw_image->num_colors; i++) {
		png_palette[i].red   = raw_image->palette[i].red;
		png_palette[i].green = raw_image->palette[i].green;
		png_palette[i].blue  = raw_image->palette[i].blue;
	}
	png_set_PLTE(img.png, img.info, png_palette, raw_image->num_colors);
	free(png_palette);

	if (opts->fix)
		set_text(&img, png_options);

	png_write_info(img.png, img.info);

	png_write_image(img.png, (png_byte **) raw_image->data);
	png_write_end(img.png, NULL);

	png_destroy_write_struct(&img.png, &img.info);
	fclose(f);

	if (opts->debug)
		free(outfile);
}

void destroy_raw_image(struct RawIndexedImage **raw_image_ptr_ptr)
{
	int y;
	struct RawIndexedImage *raw_image = *raw_image_ptr_ptr;

	for (y = 0; y < raw_image->height; y++)
		free(raw_image->data[y]);

	free(raw_image->data);
	free(raw_image->palette);
	free(raw_image);
	*raw_image_ptr_ptr = NULL;
}

static void initialize_png(struct PNGImage *img, FILE *f)
{
	img->png = png_create_read_struct(PNG_LIBPNG_VER_STRING,
					  NULL, NULL, NULL);
	if (!img->png)
		errx(1, "Creating png structure failed");

	img->info = png_create_info_struct(img->png);
	if (!img->info)
		errx(1, "Creating png info structure failed");

	if (setjmp(png_jmpbuf(img->png)))
		exit(1);

	png_init_io(img->png, f);

	png_read_info(img->png, img->info);

	img->width  = png_get_image_width(img->png, img->info);
	img->height = png_get_image_height(img->png, img->info);
	img->depth  = png_get_bit_depth(img->png, img->info);
	img->type   = png_get_color_type(img->png, img->info);
}

static void read_png(struct PNGImage *img);
static struct RawIndexedImage *create_raw_image(int width, int height,
						int num_colors);
static void set_raw_image_palette(struct RawIndexedImage *raw_image,
				  const png_color *palette, int num_colors);

static struct RawIndexedImage *indexed_png_to_raw(struct PNGImage *img)
{
	struct RawIndexedImage *raw_image;
	png_color *palette;
	int colors_in_PLTE;
	int colors_in_new_palette;
	png_byte *trans_alpha;
	int num_trans;
	png_color_16 *trans_color;
	png_color *original_palette;
	uint8_t *old_to_new_palette;
	int i, x, y;

	if (img->depth < 8)
		png_set_packing(img->png);

	png_get_PLTE(img->png, img->info, &palette, &colors_in_PLTE);

	raw_image = create_raw_image(img->width, img->height, colors);

	/*
	 * Transparent palette entries are removed, and the palette is
	 * collapsed. Transparent pixels are then replaced with palette index 0.
	 * This way, an indexed PNG can contain transparent pixels in *addition*
	 * to 4 normal colors.
	 */
	if (png_get_tRNS(img->png, img->info, &trans_alpha, &num_trans,
			 &trans_color)) {
		original_palette = palette;
		palette = malloc(sizeof(png_color) * colors_in_PLTE);
		colors_in_new_palette = 0;
		old_to_new_palette = malloc(sizeof(uint8_t) * colors_in_PLTE);

		for (i = 0; i < num_trans; i++) {
			if (trans_alpha[i] == 0) {
				old_to_new_palette[i] = 0;
			} else {
				old_to_new_palette[i] = colors_in_new_palette;
				palette[colors_in_new_palette++] =
					original_palette[i];
			}
		}
		for (i = num_trans; i < colors_in_PLTE; i++) {
			old_to_new_palette[i] = colors_in_new_palette;
			palette[colors_in_new_palette++] = original_palette[i];
		}

		if (colors_in_new_palette != colors_in_PLTE) {
			palette = realloc(palette,
					  sizeof(png_color) *
					  colors_in_new_palette);
		}

		/*
		 * Setting and validating palette before reading
		 * allows us to error out *before* doing the data
		 * transformation if the palette is too long.
		 */
		set_raw_image_palette(raw_image, palette,
				      colors_in_new_palette);
		read_png(img);

		for (y = 0; y < img->height; y++) {
			for (x = 0; x < img->width; x++) {
				raw_image->data[y][x] =
					old_to_new_palette[img->data[y][x]];
			}
		}

		free(old_to_new_palette);
	} else {
		set_raw_image_palette(raw_image, palette, colors_in_PLTE);
		read_png(img);

		for (y = 0; y < img->height; y++) {
			for (x = 0; x < img->width; x++)
				raw_image->data[y][x] = img->data[y][x];
		}
	}

	return raw_image;
}

static struct RawIndexedImage *grayscale_png_to_raw(struct PNGImage *img)
{
	if (img->depth < 8)
		png_set_expand_gray_1_2_4_to_8(img->png);

	png_set_gray_to_rgb(img->png);
	return truecolor_png_to_raw(img);
}

static void rgba_png_palette(struct PNGImage *img,
			     png_color **palette_ptr_ptr, int *num_colors);
static struct RawIndexedImage
	*processed_rgba_png_to_raw(const struct PNGImage *img,
				   const png_color *palette,
				   int colors_in_palette);

static struct RawIndexedImage *truecolor_png_to_raw(struct PNGImage *img)
{
	struct RawIndexedImage *raw_image;
	png_color *palette;
	int colors_in_palette;

	if (img->depth == 16) {
#if PNG_LIBPNG_VER >= 10504
		png_set_scale_16(img->png);
#else
		png_set_strip_16(img->png);
#endif
	}

	if (!(img->type & PNG_COLOR_MASK_ALPHA)) {
		if (png_get_valid(img->png, img->info, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(img->png);
		else
			png_set_add_alpha(img->png, 0xFF, PNG_FILLER_AFTER);
	}

	read_png(img);

	rgba_png_palette(img, &palette, &colors_in_palette);
	raw_image = processed_rgba_png_to_raw(img, palette, colors_in_palette);

	free(palette);

	return raw_image;
}

static void rgba_PLTE_palette(struct PNGImage *img,
			      png_color **palette_ptr_ptr, int *num_colors);
static void rgba_build_palette(struct PNGImage *img,
			       png_color **palette_ptr_ptr, int *num_colors);

static void rgba_png_palette(struct PNGImage *img,
			     png_color **palette_ptr_ptr, int *num_colors)
{
	if (png_get_valid(img->png, img->info, PNG_INFO_PLTE))
		rgba_PLTE_palette(img, palette_ptr_ptr, num_colors);
	else
		rgba_build_palette(img, palette_ptr_ptr, num_colors);
}

static void rgba_PLTE_palette(struct PNGImage *img,
			      png_color **palette_ptr_ptr, int *num_colors)
{
	png_get_PLTE(img->png, img->info, palette_ptr_ptr, num_colors);
	/*
	 * Lets us free the palette manually instead of leaving it to libpng,
	 * which lets us handle a PLTE and a built palette the same way.
	 */
	png_data_freer(img->png, img->info,
		       PNG_USER_WILL_FREE_DATA, PNG_FREE_PLTE);
}

static void update_built_palette(png_color *palette,
				 const png_color *pixel_color, png_byte alpha,
				 int *num_colors, bool *only_grayscale);
static int fit_grayscale_palette(png_color *palette, int *num_colors);
static void order_color_palette(png_color *palette, int num_colors);

static void rgba_build_palette(struct PNGImage *img,
			       png_color **palette_ptr_ptr, int *num_colors)
{
	png_color *palette;
	int y, value_index;
	png_color cur_pixel_color;
	png_byte cur_alpha;
	bool only_grayscale = true;

	/*
	 * By filling the palette up with black by default, if the image
	 * doesn't have enough colors, the palette gets padded with black.
	 */
	*palette_ptr_ptr = calloc(colors, sizeof(png_color));
	palette = *palette_ptr_ptr;
	*num_colors = 0;

	for (y = 0; y < img->height; y++) {
		value_index = 0;
		while (value_index < img->width * 4) {
			cur_pixel_color.red   = img->data[y][value_index++];
			cur_pixel_color.green = img->data[y][value_index++];
			cur_pixel_color.blue  = img->data[y][value_index++];
			cur_alpha = img->data[y][value_index++];

			update_built_palette(palette, &cur_pixel_color,
					     cur_alpha,
					     num_colors, &only_grayscale);
		}
	}

	/* In order not to count 100% transparent images as grayscale. */
	only_grayscale = *num_colors ? only_grayscale : false;

	if (!only_grayscale || !fit_grayscale_palette(palette, num_colors))
		order_color_palette(palette, *num_colors);
}

static void update_built_palette(png_color *palette,
				 const png_color *pixel_color, png_byte alpha,
				 int *num_colors, bool *only_grayscale)
{
	bool color_exists;
	png_color cur_palette_color;
	int i;

	/*
	 * Transparent pixels don't count toward the palette,
	 * as they'll be replaced with color #0 later.
	 */
	if (alpha == 0)
		return;

	if (*only_grayscale && !(pixel_color->red == pixel_color->green &&
				 pixel_color->red == pixel_color->blue)) {
		*only_grayscale = false;
	}

	color_exists = false;
	for (i = 0; i < *num_colors; i++) {
		cur_palette_color = palette[i];
		if (pixel_color->red   == cur_palette_color.red   &&
		    pixel_color->green == cur_palette_color.green &&
		    pixel_color->blue  == cur_palette_color.blue) {
			color_exists = true;
			break;
		}
	}
	if (!color_exists) {
		if (*num_colors == colors) {
			err(1, "Too many colors in input PNG file to fit into a %d-bit palette (max %d).",
			    depth, colors);
		}
		palette[*num_colors] = *pixel_color;
		(*num_colors)++;
	}
}

static int fit_grayscale_palette(png_color *palette, int *num_colors)
{
	int interval = 256 / colors;
	png_color *fitted_palette = malloc(sizeof(png_color) * colors);
	bool *set_indices = calloc(colors, sizeof(bool));
	int i, shade_index;

	fitted_palette[0].red   = 0xFF;
	fitted_palette[0].green = 0xFF;
	fitted_palette[0].blue  = 0xFF;
	fitted_palette[colors - 1].red   = 0;
	fitted_palette[colors - 1].green = 0;
	fitted_palette[colors - 1].blue  = 0;
	if (colors == 4) {
		fitted_palette[1].red   = 0xA9;
		fitted_palette[1].green = 0xA9;
		fitted_palette[1].blue  = 0xA9;
		fitted_palette[2].red   = 0x55;
		fitted_palette[2].green = 0x55;
		fitted_palette[2].blue  = 0x55;
	}

	for (i = 0; i < *num_colors; i++) {
		shade_index = colors - 1 - palette[i].red / interval;
		if (set_indices[shade_index]) {
			free(fitted_palette);
			free(set_indices);
			return false;
		}
		fitted_palette[shade_index] = palette[i];
		set_indices[shade_index] = true;
	}

	for (i = 0; i < colors; i++)
		palette[i] = fitted_palette[i];

	*num_colors = colors;

	free(fitted_palette);
	free(set_indices);
	return true;
}

/* A combined struct is needed to sort csolors in order of luminance. */
struct ColorWithLuminance {
	png_color color;
	int luminance;
};

static int compare_luminance(const void *a, const void *b)
{
	const struct ColorWithLuminance *x, *y;

	x = (const struct ColorWithLuminance *)a;
	y = (const struct ColorWithLuminance *)b;

	return y->luminance - x->luminance;
}

static void order_color_palette(png_color *palette, int num_colors)
{
	int i;
	struct ColorWithLuminance *palette_with_luminance =
		malloc(sizeof(struct ColorWithLuminance) * num_colors);

	for (i = 0; i < num_colors; i++) {
		/*
		 * Normally this would be done with floats, but since it's only
		 * used for comparison, we might as well use integer math.
		 */
		palette_with_luminance[i].color = palette[i];
		palette_with_luminance[i].luminance = 2126 * palette[i].red   +
						      7152 * palette[i].green +
						       722 * palette[i].blue;
	}
	qsort(palette_with_luminance, num_colors,
	      sizeof(struct ColorWithLuminance), compare_luminance);
	for (i = 0; i < num_colors; i++)
		palette[i] = palette_with_luminance[i].color;

	free(palette_with_luminance);
}

static void put_raw_image_pixel(struct RawIndexedImage *raw_image,
				const struct PNGImage *img,
				int *value_index, int x, int y,
				const png_color *palette,
				int colors_in_palette);

static struct RawIndexedImage
	*processed_rgba_png_to_raw(const struct PNGImage *img,
				   const png_color *palette,
				   int colors_in_palette)
{
	struct RawIndexedImage *raw_image;
	int x, y, value_index;

	raw_image = create_raw_image(img->width, img->height, colors);

	set_raw_image_palette(raw_image, palette, colors_in_palette);

	for (y = 0; y < img->height; y++) {
		x = raw_image->width - 1;
		value_index = img->width * 4 - 1;

		while (x >= 0) {
			put_raw_image_pixel(raw_image, img,
					    &value_index, x, y,
					    palette, colors_in_palette);
			x--;
		}
	}

	return raw_image;
}

static uint8_t palette_index_of(const png_color *palette,
				int num_colors, const png_color *color);

static void put_raw_image_pixel(struct RawIndexedImage *raw_image,
				const struct PNGImage *img,
				int *value_index, int x, int y,
				const png_color *palette,
				int colors_in_palette)
{
	png_color pixel_color;
	png_byte alpha;

	alpha = img->data[y][*value_index];
	if (alpha == 0) {
		raw_image->data[y][x] = 0;
		*value_index -= 4;
	} else {
		(*value_index)--;
		pixel_color.blue  = img->data[y][(*value_index)--];
		pixel_color.green = img->data[y][(*value_index)--];
		pixel_color.red   = img->data[y][(*value_index)--];
		raw_image->data[y][x] = palette_index_of(palette,
							 colors_in_palette,
							 &pixel_color);
	}
}

static uint8_t palette_index_of(const png_color *palette,
				int num_colors, const png_color *color)
{
	uint8_t i;

	for (i = 0; i < num_colors; i++) {
		if (palette[i].red   == color->red   &&
		    palette[i].green == color->green &&
		    palette[i].blue  == color->blue) {
			return i;
		}
	}
	errx(1, "The input PNG file contains colors that don't appear in its embedded palette.");
}

static void read_png(struct PNGImage *img)
{
	int y;

	png_read_update_info(img->png, img->info);

	img->data = malloc(sizeof(png_byte *) * img->height);
	for (y = 0; y < img->height; y++)
		img->data[y] = malloc(png_get_rowbytes(img->png, img->info));

	png_read_image(img->png, img->data);
	png_read_end(img->png, img->info);
}

static struct RawIndexedImage *create_raw_image(int width, int height,
						int num_colors)
{
	struct RawIndexedImage *raw_image;
	int y;

	raw_image = malloc(sizeof(struct RawIndexedImage));

	raw_image->width = width;
	raw_image->height = height;
	raw_image->num_colors = num_colors;

	raw_image->palette = malloc(sizeof(struct RGBColor) * num_colors);

	raw_image->data = malloc(sizeof(uint8_t *) * height);
	for (y = 0; y < height; y++)
		raw_image->data[y] = malloc(sizeof(uint8_t) * width);

	return raw_image;
}

static void set_raw_image_palette(struct RawIndexedImage *raw_image,
				  const png_color *palette, int num_colors)
{
	int i;

	if (num_colors > raw_image->num_colors) {
		errx(1, "Too many colors in input PNG file's palette to fit into a %d-bit palette (%d in input palette, max %d).",
		     raw_image->num_colors >> 1,
		     num_colors, raw_image->num_colors);
	}

	for (i = 0; i < num_colors; i++) {
		raw_image->palette[i].red   = palette[i].red;
		raw_image->palette[i].green = palette[i].green;
		raw_image->palette[i].blue  = palette[i].blue;
	}
	for (i = num_colors; i < raw_image->num_colors; i++) {
		raw_image->palette[i].red   = 0;
		raw_image->palette[i].green = 0;
		raw_image->palette[i].blue  = 0;
	}
}

static void get_text(const struct PNGImage *img,
		     struct ImageOptions *png_options)
{
	png_text *text;
	int i, numtxts, numremoved;

	png_get_text(img->png, img->info, &text, &numtxts);
	for (i = 0; i < numtxts; i++) {
		if (strcmp(text[i].key, "h") == 0 && !*text[i].text) {
			png_options->horizontal = true;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "x") == 0) {
			png_options->trim = strtoul(text[i].text, NULL, 0);
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "t") == 0) {
			png_options->tilemapfile = text[i].text;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "T") == 0 && !*text[i].text) {
			png_options->tilemapout = true;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "a") == 0) {
			png_options->attrmapfile = text[i].text;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "A") == 0 && !*text[i].text) {
			png_options->attrmapout = true;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "p") == 0) {
			png_options->palfile = text[i].text;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		} else if (strcmp(text[i].key, "P") == 0 && !*text[i].text) {
			png_options->palout = true;
			png_free_data(img->png, img->info, PNG_FREE_TEXT, i);
		}
	}

	/*
	 * TODO: Remove this and simply change the warning function not to warn
	 * instead.
	 */
	for (i = 0, numremoved = 0; i < numtxts; i++) {
		if (text[i].key == NULL)
			numremoved++;

		text[i].key = text[i + numremoved].key;
		text[i].text = text[i + numremoved].text;
		text[i].compression = text[i + numremoved].compression;
	}
	png_set_text(img->png, img->info, text, numtxts - numremoved);
}

static void set_text(const struct PNGImage *img,
		     const struct ImageOptions *png_options)
{
	png_text *text;
	char buffer[3];

	text = malloc(sizeof(png_text));

	if (png_options->horizontal) {
		text[0].key = "h";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (png_options->trim) {
		text[0].key = "x";
		snprintf(buffer, 3, "%d", png_options->trim);
		text[0].text = buffer;
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (*png_options->tilemapfile) {
		text[0].key = "t";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (png_options->tilemapout) {
		text[0].key = "T";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (*png_options->attrmapfile) {
		text[0].key = "a";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (png_options->attrmapout) {
		text[0].key = "A";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (*png_options->palfile) {
		text[0].key = "p";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}
	if (png_options->palout) {
		text[0].key = "P";
		text[0].text = "";
		text[0].compression = PNG_TEXT_COMPRESSION_NONE;
		png_set_text(img->png, img->info, text, 1);
	}

	free(text);
}

static void free_png_data(const struct PNGImage *img)
{
	int y;

	for (y = 0; y < img->height; y++)
		free(img->data[y]);

	free(img->data);
}
