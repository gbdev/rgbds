/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINKDEFS_H
#define RGBDS_LINKDEFS_H

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

enum SymbolType {
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

#endif /* RGBDS_LINKDEFS_H */
