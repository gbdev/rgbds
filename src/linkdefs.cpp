// SPDX-License-Identifier: MIT

#include "linkdefs.hpp"

#include <string>

using namespace std::literals;

// The default values are the most lax, as they are used as-is by RGBASM; only RGBLINK has the full
// info, so RGBASM's job is only to catch unconditional errors earlier.
// clang-format off: nested initializers
SectionTypeInfo sectionTypeInfo[SECTTYPE_INVALID] = {
    {
        .name = "WRAM0"s,
        .firstBank = 0,
        .lastBank = 0,
        .startAddr = 0xC000,
        .size = 0x2000, // Patched to 0x1000 if !isWRAM0Mode
    },
    {
        .name = "VRAM"s,
        .firstBank = 0,
        .lastBank = 1, // Patched to 0 if isDmgMode
        .startAddr = 0x8000,
        .size = 0x2000,
    },
    {
        .name = "ROMX"s,
        .firstBank = 1,
        .lastBank = 65535,
        .startAddr = 0x4000,
        .size = 0x4000,
    },
    {
        .name = "ROM0"s,
        .firstBank = 0,
        .lastBank = 0,
        .startAddr = 0x0000,
        .size = 0x8000, // Patched to 0x4000 if !is32kMode
    },
    {
        .name = "HRAM"s,
        .firstBank = 0,
        .lastBank = 0,
        .startAddr = 0xFF80,
        .size = 0x007F,
    },
    {
        .name = "WRAMX"s,
        .firstBank = 1,
        .lastBank = 7,
        .startAddr = 0xD000,
        .size = 0x1000,
    },
    {
        .name = "SRAM"s,
        .firstBank = 0,
        .lastBank = 255,
        .startAddr = 0xA000,
        .size = 0x2000,
    },
    {
        .name = "OAM"s,
        .firstBank = 0,
        .lastBank = 0,
        .startAddr = 0xFE00,
        .size = 0x00A0,
    },
};
// clang-format on

char const * const sectionModNames[] = {
    "(no modifier)", // SECTION_NORMAL
    "UNION",         // SECTION_UNION
    "FRAGMENT",      // SECTION_FRAGMENT
};
