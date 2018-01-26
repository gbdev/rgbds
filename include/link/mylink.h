/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINK_LINK_H
#define RGBDS_LINK_LINK_H

#include <stdint.h>

#include "linkdefs.h"

extern int32_t options;

#define OPT_TINY		0x01
#define OPT_SMART_C_LINK	0x02
#define OPT_OVERLAY		0x04
#define OPT_CONTWRAM		0x08
#define OPT_DMG_MODE		0x10

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

struct sSymbol {
	char *pzName;
	enum eSymbolType Type;

	/* The following 3 items only valid when Type!=SYM_IMPORT */
	int32_t nSectionID; /* Internal to object.c */
	struct sSection *pSection;
	int32_t nOffset;

	char *pzObjFileName; /* Object file where the symbol is located. */
	char *pzFileName; /* Source file where the symbol was defined. */
	uint32_t nFileLine; /* Line where the symbol was defined. */
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

#endif /* RGBDS_LINK_LINK_H */
