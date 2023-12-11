/* SPDX-License-Identifier: MIT */

#include "linkdefs.hpp"
#include "platform.hpp"

using namespace std::literals;

// The default values are the most lax, as they are used as-is by RGBASM; only RGBLINK has the full info,
// so RGBASM's job is only to catch unconditional errors earlier.
struct SectionTypeInfo sectionTypeInfo[SECTTYPE_INVALID] = {
	AT(SECTTYPE_WRAM0) {
		.name = "WRAM0"s,
		.startAddr = 0xC000,
		.size = 0x2000, // Patched to 0x1000 if !isWRA0Mode
		.firstBank = 0,
		.lastBank = 0,
	},
	AT(SECTTYPE_VRAM) {
		.name = "VRAM"s,
		.startAddr = 0x8000,
		.size = 0x2000,
		.firstBank = 0,
		.lastBank = 1, // Patched to 0 if isDmgMode
	},
	AT(SECTTYPE_ROMX) {
		.name = "ROMX"s,
		.startAddr = 0x4000,
		.size = 0x4000,
		.firstBank = 1,
		.lastBank = 65535,
	},
	AT(SECTTYPE_ROM0) {
		.name = "ROM0"s,
		.startAddr = 0x0000,
		.size = 0x8000, // Patched to 0x4000 if !is32kMode
		.firstBank = 0,
		.lastBank = 0,
	},
	AT(SECTTYPE_HRAM) {
		.name = "HRAM"s,
		.startAddr = 0xFF80,
		.size = 0x007F,
		.firstBank = 0,
		.lastBank = 0,
	},
	AT(SECTTYPE_WRAMX) {
		.name = "WRAMX"s,
		.startAddr = 0xD000,
		.size = 0x1000,
		.firstBank = 1,
		.lastBank = 7,
	},
	AT(SECTTYPE_SRAM) {
		.name = "SRAM"s,
		.startAddr = 0xA000,
		.size = 0x2000,
		.firstBank = 0,
		.lastBank = 255,
	},
	AT(SECTTYPE_OAM) {
		.name = "OAM"s,
		.startAddr = 0xFE00,
		.size = 0x00A0,
		.firstBank = 0,
		.lastBank = 0,
	},
};

char const * const sectionModNames[] = {
	AT(SECTION_NORMAL)   "regular",
	AT(SECTION_UNION)    "union",
	AT(SECTION_FRAGMENT) "fragment",
};
