/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2013-2018, stag019 and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_GFX_PNG_H
#define RGBDS_GFX_PNG_H

#include "gfx/main.h"

struct RawIndexedImage *input_png_file(const struct Options *opts,
				       struct ImageOptions *png_options);
void output_png_file(const struct Options *opts,
		     const struct ImageOptions *png_options,
		     const struct RawIndexedImage *raw_image);
void destroy_raw_image(struct RawIndexedImage **raw_image_ptr_ptr);

#endif /* RGBDS_GFX_PNG_H */
