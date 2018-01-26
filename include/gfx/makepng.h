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

void input_png_file(const struct Options opts, struct PNGImage *img);
void get_text(struct PNGImage *png);
void set_text(const struct PNGImage *png);
void output_png_file(const struct Options opts, const struct PNGImage *png);
void free_png_data(const struct PNGImage *png);

#endif /* RGBDS_GFX_PNG_H */
