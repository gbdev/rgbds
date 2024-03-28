/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_GFX_PAL_SORTING_HPP
#define RGBDS_GFX_PAL_SORTING_HPP

#include <array>
#include <optional>
#include <png.h>
#include <vector>

#include "gfx/rgba.hpp"

struct Palette;

namespace sorting {

void indexed(
    std::vector<Palette> &palettes,
    int palSize,
    png_color const *palRGB,
    int palAlphaSize,
    png_byte *palAlpha
);
void grayscale(
    std::vector<Palette> &palettes, std::array<std::optional<Rgba>, 0x8001> const &colors
);
void rgb(std::vector<Palette> &palettes);

} // namespace sorting

#endif // RGBDS_GFX_PAL_SORTING_HPP
