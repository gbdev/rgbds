#ifndef RGBDS_LINK_LINK_H
#define RGBDS_LINK_LINK_H

#ifndef _MAX_PATH
#define _MAX_PATH	512
#endif

#include "types.h"

extern SLONG options;
#define OPT_SMALL		0x01
#define OPT_SMART_C_LINK	0x02
#define OPT_OVERLAY		0x04
#define OPT_CONTWRAM		0x08

enum eRpnData {
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

enum eSectionType {
	SECT_WRAM0,
	SECT_VRAM,
	SECT_ROMX,
	SECT_ROM0,
	SECT_HRAM,
	SECT_WRAMX,
	SECT_SRAM,
	SECT_OAM
};

struct sSection {
	SLONG nBank;
	SLONG nOrg;
	SLONG nAlign;
	BBOOL oAssigned;

	char *pzName;
	SLONG nByteSize;
	enum eSectionType Type;
	UBYTE *pData;
	SLONG nNumberOfSymbols;
	struct sSymbol **tSymbols;
	struct sPatch *pPatches;
	struct sSection *pNext;
};

enum eSymbolType {
	SYM_LOCAL,
	SYM_IMPORT,
	SYM_EXPORT
};

struct sSymbol {
	char *pzName;
	enum eSymbolType Type;
	/* the following 3 items only valid when Type!=SYM_IMPORT */
	SLONG nSectionID;	/* internal to object.c */
	struct sSection *pSection;
	SLONG nOffset;
};

enum ePatchType {
	PATCH_BYTE = 0,
	PATCH_WORD_L,
	PATCH_LONG_L,
	PATCH_WORD_B,
	PATCH_LONG_B
};

struct sPatch {
	char *pzFilename;
	SLONG nLineNo;
	SLONG nOffset;
	enum ePatchType Type;
	SLONG nRPNSize;
	UBYTE *pRPN;
	struct sPatch *pNext;
	BBOOL oRelocPatch;
};

extern struct sSection *pSections;
extern struct sSection *pLibSections;

#endif
