// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PAL_SPEC_HPP
#define RGBDS_GFX_PAL_SPEC_HPP

#include <stdint.h>
#include <string>

#include "gfx/png.hpp"

void parseInlinePalSpec(char const * const rawArg);
void parseEmbeddedPalSpec(Png const &png);
void parseExternalPalSpec(char const *arg);
void parseDmgPalSpec(char const * const rawArg);
void parseBackgroundPalSpec(char const *arg);

#endif // RGBDS_GFX_PAL_SPEC_HPP
