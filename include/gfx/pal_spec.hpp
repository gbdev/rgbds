// SPDX-License-Identifier: MIT

#ifndef RGBDS_GFX_PAL_SPEC_HPP
#define RGBDS_GFX_PAL_SPEC_HPP

#include <stdint.h>

void parseInlinePalSpec(char const * const rawArg);
void parseExternalPalSpec(char const *arg);
void parseDmgPalSpec(char const * const rawArg);
void parseDmgPalSpec(uint8_t palSpecDmg);

void parseBackgroundPalSpec(char const *arg);

#endif // RGBDS_GFX_PAL_SPEC_HPP
