// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PAL_SORTING_HPP
#define RGBDS_GFX_PAL_SORTING_HPP

#include <array>
#include <optional>
#include <stddef.h>
#include <vector>

#include "gfx/rgba.hpp"

// Allow a slot for every possible CGB color, plus one for transparency
// 32 (1 << 5) per channel, times 3 RGB channels = 32768 CGB colors
static constexpr size_t NB_COLOR_SLOTS = (1 << (5 * 3)) + 1;

struct Palette;

void sortIndexed(std::vector<Palette> &palettes, std::vector<Rgba> const &embPal);
void sortGrayscale(
    std::vector<Palette> &palettes, std::array<std::optional<Rgba>, NB_COLOR_SLOTS> const &colors
);
void sortRgb(std::vector<Palette> &palettes);

#endif // RGBDS_GFX_PAL_SORTING_HPP
