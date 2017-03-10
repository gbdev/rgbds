#ifndef RGBDS_ASM_LINK_H
#define RGBDS_ASM_LINK_H

/* RGB0 .obj format:
 *
 * Header
 * Symbols
 * Sections
 *
 * Header:
 * "RGB0"
 * LONG NumberOfSymbols
 * LONG NumberOfSections
 *
 * Symbols:
 * Symbol[NumberOfSymbols]
 *
 * Symbol:
 * char Name (NULL terminated)
 * char nType
 * if( nType!=SYM_IMPORT )
 * {
 *		LONG SectionID
 *		LONG Offset
 * }
 *
 * Sections:
 * Section[NumberOfSections]
 *
 * Section:
 * LONG SizeInBytes
 * char Type
 * if( Type!=WRAM0 )
 * {
 *		char Data[SizeInBytes]
 *		Patches
 * }
 *
 * Patches:
 * LONG NumberOfPatches
 * Patch[NumberOfPatches]
 *
 * Patch:
 * char Filename NULL-terminated
 * LONG LineNo
 * LONG Offset
 * char Type
 * LONG RpnByteSize
 * Rpn[RpnByteSize]
 *
 * Rpn:
 * Operators: 0x00-0x7F
 * Constants: 0x80 0x00000000
 * Symbols  : 0x81 0x00000000
 *
 */

enum {
	RPN_ADD = 0,
	RPN_SUB,
	RPN_MUL,
	RPN_DIV,
	RPN_MOD,
	RPN_UNSUB,

	RPN_OR,
	RPN_AND,
	RPN_XOR,
	RPN_UNNOT,

	RPN_LOGAND,
	RPN_LOGOR,
	RPN_LOGUNNOT,

	RPN_LOGEQ,
	RPN_LOGNE,
	RPN_LOGGT,
	RPN_LOGLT,
	RPN_LOGGE,
	RPN_LOGLE,

	RPN_SHL,
	RPN_SHR,

	RPN_BANK,

	RPN_HRAM,

	RPN_PCEZP,

	RPN_RANGECHECK,

	RPN_CONST = 0x80,
	RPN_SYM = 0x81
};

enum {
	SECT_WRAM0 = 0,
	SECT_VRAM,
	SECT_ROMX,
	SECT_ROM0,
	SECT_HRAM,
	SECT_WRAMX,
	SECT_SRAM,
	SECT_OAM
};

enum {
	SYM_LOCAL = 0,
	SYM_IMPORT,
	SYM_EXPORT
};

enum {
	PATCH_BYTE = 0,
	PATCH_WORD_L,
	PATCH_LONG_L,
	PATCH_WORD_B,
	PATCH_LONG_B
};
#endif
