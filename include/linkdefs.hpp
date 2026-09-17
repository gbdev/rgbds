// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINKDEFS_HPP
#define RGBDS_LINKDEFS_HPP

#include <stdint.h>
#include <string_view>

#include "helpers.hpp" // assume

#define RGBDS_OBJECT_VERSION_STRING "RGB9"
#define RGBDS_OBJECT_REV            13U

enum AssertionType { ASSERT_WARN, ASSERT_ERROR, ASSERT_FATAL };

enum RPNCommand {
	RPN_ADD = 0x00,
	RPN_SUB = 0x01,
	RPN_MUL = 0x02,
	RPN_DIV = 0x03,
	RPN_MOD = 0x04,
	RPN_NEG = 0x05,
	RPN_EXP = 0x06,

	RPN_OR = 0x10,
	RPN_AND = 0x11,
	RPN_XOR = 0x12,
	RPN_NOT = 0x13,

	RPN_LOGAND = 0x21,
	RPN_LOGOR = 0x22,
	RPN_LOGNOT = 0x23,

	RPN_LOGEQ = 0x30,
	RPN_LOGNE = 0x31,
	RPN_LOGGT = 0x32,
	RPN_LOGLT = 0x33,
	RPN_LOGGE = 0x34,
	RPN_LOGLE = 0x35,

	RPN_SHL = 0x40,
	RPN_SHR = 0x41,
	RPN_USHR = 0x42,

	RPN_BANK_SYM = 0x50,
	RPN_BANK_SECT = 0x51,
	RPN_BANK_SELF = 0x52,
	RPN_SIZEOF_SECT = 0x53,
	RPN_STARTOF_SECT = 0x54,
	RPN_SIZEOF_SECTTYPE = 0x55,
	RPN_STARTOF_SECTTYPE = 0x56,

	RPN_HRAM = 0x60,
	RPN_RST = 0x61,
	RPN_BIT_INDEX = 0x62,

	RPN_HIGH = 0x70,
	RPN_LOW = 0x71,
	RPN_BITWIDTH = 0x72,
	RPN_TZCOUNT = 0x73,

	RPN_CONST = 0x80,
	RPN_SYM = 0x81
};

enum SectionType {
	SECTTYPE_WRAM0,
	SECTTYPE_VRAM,
	SECTTYPE_ROMX,
	SECTTYPE_ROM0,
	SECTTYPE_HRAM,
	SECTTYPE_WRAMX,
	SECTTYPE_SRAM,
	SECTTYPE_OAM,

	// In RGBLINK, this is used for "indeterminate" sections; this is primarily for SDCC
	// areas, which do not carry any section type info and must be told from the linker script
	SECTTYPE_INVALID
};

static constexpr uint8_t SECTTYPE_TYPE_MASK = 0b111;
static constexpr uint8_t SECTTYPE_UNION_BIT = 7;
static constexpr uint8_t SECTTYPE_FRAGMENT_BIT = 6;

// Tells whether a section has data in its object file definition,
// depending on type.
static inline bool sectTypeHasData(SectionType type) {
	assume(type != SECTTYPE_INVALID);
	return type == SECTTYPE_ROM0 || type == SECTTYPE_ROMX;
}

enum FileStackNodeType {
	NODE_REPT,
	NODE_FILE,
	NODE_MACRO,
};

static constexpr uint8_t FSTACKNODE_QUIET_BIT = 7;

// Non-`const` members may be patched in RGBLINK depending on CLI flags
struct SectionTypeInfo {
	std::string_view const name;
	uint16_t const startAddr;
	uint16_t size;
	uint32_t const firstBank;
	uint32_t lastBank;

	// Returns a memory region's end address (last byte), e.g. 0x7FFF
	uint16_t endAddr() const { return startAddr + size - 1; }

	// Returns a memory region's number of banks, or 1 for regions without banking
	uint32_t nbBanks() const { return lastBank - firstBank + 1; }

	bool isBanked() const { return nbBanks() != 1; }
};

extern SectionTypeInfo sectionTypeInfo[SECTTYPE_INVALID];

enum SectionModifier { SECTION_NORMAL, SECTION_UNION, SECTION_FRAGMENT };

extern char const * const sectionModNames[];

enum ExportLevel { SYMTYPE_LOCAL, SYMTYPE_IMPORT, SYMTYPE_EXPORT, SYMTYPE_INVALID };

enum PatchType {
	PATCHTYPE_BYTE,
	PATCHTYPE_WORD,
	PATCHTYPE_LONG,
	PATCHTYPE_JR,

	PATCHTYPE_INVALID
};

#endif // RGBDS_LINKDEFS_HPP
