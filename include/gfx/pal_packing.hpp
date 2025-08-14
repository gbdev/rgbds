// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PAL_PACKING_HPP
#define RGBDS_GFX_PAL_PACKING_HPP

#include <stddef.h>
#include <utility>
#include <vector>

struct Palette;
class ColorSet;

// Returns which palette each color set maps to, and how many palettes are necessary
std::pair<std::vector<size_t>, size_t> overloadAndRemove(std::vector<ColorSet> const &colorSets);

#endif // RGBDS_GFX_PAL_PACKING_HPP
