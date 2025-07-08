// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PAL_PACKING_HPP
#define RGBDS_GFX_PAL_PACKING_HPP

#include <stddef.h>
#include <tuple>
#include <vector>

struct Palette;
class ProtoPalette;

// Returns which palette each proto-palette maps to, and how many palettes are necessary
std::tuple<std::vector<size_t>, size_t>
    overloadAndRemove(std::vector<ProtoPalette> const &protoPalettes);

#endif // RGBDS_GFX_PAL_PACKING_HPP
