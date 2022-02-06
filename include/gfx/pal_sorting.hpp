/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2022, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_GFX_PAL_SORTING_HPP
#define RGBDS_GFX_PAL_SORTING_HPP

#include <png.h>
#include <vector>

class Palette;

namespace sorting {

void indexed(std::vector<Palette> &palettes, int palSize, png_color const *palRGB,
             png_byte *palAlpha);
void grayscale(std::vector<Palette> &palettes);
void rgb(std::vector<Palette> &palettes);

}

#endif /* RGBDS_GFX_PAL_SORTING_HPP */
