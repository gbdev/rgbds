#ifndef RGBDS_LINK_LINK_H
#define RGBDS_LINK_LINK_H

#include <stdint.h>

extern int32_t options;

#define OPT_TINY		0x01
#define OPT_SMART_C_LINK	0x02
#define OPT_OVERLAY		0x04
#define OPT_CONTWRAM		0x08
#define OPT_DMG_MODE		0x10

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
	int32_t nBank;
	int32_t nOrg;
	int32_t nAlign;
	uint8_t oAssigned;

	char *pzName;
	int32_t nByteSize;
	enum eSectionType Type;
	uint8_t *pData;
	int32_t nNumberOfSymbols;
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
	int32_t nSectionID;	/* internal to object.c */
	struct sSection *pSection;
	int32_t nOffset;
	char *pzObjFileName; /* Object file where the symbol is located. */
	char *pzFileName; /* Source file where the symbol was defined. */
	uint32_t nFileLine; /* Line where the symbol was defined. */
};

enum ePatchType {
	PATCH_BYTE = 0,
	PATCH_WORD_L,
	PATCH_LONG_L
};

struct sPatch {
	char *pzFilename;
	int32_t nLineNo;
	int32_t nOffset;
	enum ePatchType Type;
	int32_t nRPNSize;
	uint8_t *pRPN;
	struct sPatch *pNext;
	uint8_t oRelocPatch;
};

extern struct sSection *pSections;
extern struct sSection *pLibSections;

#endif
