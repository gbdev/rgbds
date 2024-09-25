/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_GFX_PAL_PACKING_HPP
#define RGBDS_GFX_PAL_PACKING_HPP

#include <tuple>
#include <vector>

#include "defaultinitvec.hpp"

struct Palette;
class ProtoPalette;

/*
 * Returns which palette each proto-palette maps to, and how many palettes are necessary
 */
std::tuple<DefaultInitVec<size_t>, size_t>
    overloadAndRemove(std::vector<ProtoPalette> const &protoPalettes);

#endif // RGBDS_GFX_PAL_PACKING_HPP
