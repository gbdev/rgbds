/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINKDEFS_H
#define RGBDS_LINKDEFS_H

#include <stdbool.h>
#include <stdint.h>

#define RGBDS_OBJECT_VERSION_STRING "RGB%1hhu"
#define RGBDS_OBJECT_VERSION_NUMBER (uint8_t)9
#define RGBDS_OBJECT_REV 1

enum RPNCommand {
	RPN_ADD		= 0x00,
	RPN_SUB		= 0x01,
	RPN_MUL		= 0x02,
	RPN_DIV		= 0x03,
	RPN_MOD		= 0x04,
	RPN_UNSUB	= 0x05,

	RPN_OR		= 0x10,
	RPN_AND		= 0x11,
	RPN_XOR		= 0x12,
	RPN_UNNOT	= 0x13,

	RPN_LOGAND	= 0x21,
	RPN_LOGOR	= 0x22,
	RPN_LOGUNNOT	= 0x23,

	RPN_LOGEQ	= 0x30,
	RPN_LOGNE	= 0x31,
	RPN_LOGGT	= 0x32,
	RPN_LOGLT	= 0x33,
	RPN_LOGGE	= 0x34,
	RPN_LOGLE	= 0x35,

	RPN_SHL		= 0x40,
	RPN_SHR		= 0x41,

	RPN_BANK_SYM	= 0x50,
	RPN_BANK_SECT	= 0x51,
	RPN_BANK_SELF	= 0x52,

	RPN_HRAM	= 0x60,
	RPN_RST         = 0x61,

	RPN_CONST	= 0x80,
	RPN_SYM		= 0x81
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

	SECTTYPE_INVALID
};

/**
 * Tells whether a section has data in its object file definition,
 * depending on type.
 * @param type The section's type
 * @return `true` if the section's definition includes data
 */
static inline bool sect_HasData(enum SectionType type)
{
	return type == SECTTYPE_ROM0 || type == SECTTYPE_ROMX;
}

enum ExportLevel {
	SYMTYPE_LOCAL,
	SYMTYPE_IMPORT,
	SYMTYPE_EXPORT
};

enum PatchType {
	PATCHTYPE_BYTE,
	PATCHTYPE_WORD,
	PATCHTYPE_LONG,
	PATCHTYPE_JR,

	PATCHTYPE_INVALID
};

#define BANK_MIN_ROM0  0
#define BANK_MAX_ROM0  0
#define BANK_MIN_ROMX  1
#define BANK_MAX_ROMX  511
#define BANK_MIN_VRAM  0
#define BANK_MAX_VRAM  1
#define BANK_MIN_SRAM  0
#define BANK_MAX_SRAM  15
#define BANK_MIN_WRAM0 0
#define BANK_MAX_WRAM0 0
#define BANK_MIN_WRAMX 1
#define BANK_MAX_WRAMX 7
#define BANK_MIN_OAM   0
#define BANK_MAX_OAM   0
#define BANK_MIN_HRAM  0
#define BANK_MAX_HRAM  0

extern uint16_t startaddr[];
extern uint16_t maxsize[];
extern uint32_t bankranges[][2];

/**
 * Computes a memory region's end address (last byte), eg. 0x7FFF
 * @return The address of the last byte in that memory region
 */
static inline uint16_t endaddr(enum SectionType type)
{
	return startaddr[type] + maxsize[type] - 1;
}

/**
 * Computes a memory region's number of banks
 * @return The number of banks, 1 for regions without banking
 */
static inline uint32_t nbbanks(enum SectionType type)
{
	return bankranges[type][1] - bankranges[type][0] + 1;
}

extern char const * const typeNames[SECTTYPE_INVALID];

#endif /* RGBDS_LINKDEFS_H */
