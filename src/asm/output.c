/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2019, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Outputs an objectfile
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/fstack.h"
#include "asm/main.h"
#include "asm/output.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/warning.h"

#include "extern/err.h"

#include "linkdefs.h"

struct Patch {
	char tzFilename[_MAX_PATH + 1];
	uint32_t nOffset;
	uint8_t nType;
	uint32_t nRPNSize;
	uint8_t *pRPN;
	struct Patch *pNext;
};

struct PatchSymbol {
	uint32_t ID;
	struct sSymbol const *pSymbol;
	struct PatchSymbol *pNext;
	struct PatchSymbol *pBucketNext; /* next symbol in hash table bucket */
};

struct PatchSymbol *tHashedPatchSymbols[HASHSIZE];
struct Section *pSectionList, *pCurrentSection;
struct PatchSymbol *pPatchSymbols;
struct PatchSymbol **ppPatchSymbolsTail = &pPatchSymbols;
char *tzObjectname;

/*
 * Count the number of symbols used in this object
 */
static uint32_t countsymbols(void)
{
	struct PatchSymbol *pSym;
	uint32_t count = 0;

	pSym = pPatchSymbols;
	while (pSym) {
		count++;
		pSym = pSym->pNext;
	}

	return count;
}

/*
 * Count the number of sections used in this object
 */
static uint32_t countsections(void)
{
	struct Section *pSect;
	uint32_t count = 0;

	pSect = pSectionList;
	while (pSect) {
		count++;
		pSect = pSect->pNext;
	}

	return count;
}

/*
 * Count the number of patches used in this object
 */
static uint32_t countpatches(struct Section *pSect)
{
	struct Patch *pPatch;
	uint32_t r = 0;

	pPatch = pSect->pPatches;
	while (pPatch) {
		r++;
		pPatch = pPatch->pNext;
	}

	return r;
}

/*
 * Write a long to a file (little-endian)
 */
static void fputlong(uint32_t i, FILE *f)
{
	fputc(i, f);
	fputc(i >> 8, f);
	fputc(i >> 16, f);
	fputc(i >> 24, f);
}

/*
 * Write a NULL-terminated string to a file
 */
static void fputstring(char const *s, FILE *f)
{
	while (*s)
		fputc(*s++, f);
	fputc(0, f);
}

/*
 * Return a section's ID
 */
static uint32_t getsectid(struct Section *pSect)
{
	struct Section *sec;
	uint32_t ID = 0;

	sec = pSectionList;

	while (sec) {
		if (sec == pSect)
			return ID;
		ID++;
		sec = sec->pNext;
	}

	fatalerror("Unknown section '%s'", pSect->pzName);
}

/*
 * Write a patch to a file
 */
static void writepatch(struct Patch *pPatch, FILE *f)
{
	fputstring(pPatch->tzFilename, f);
	fputlong(pPatch->nOffset, f);
	fputc(pPatch->nType, f);
	fputlong(pPatch->nRPNSize, f);
	fwrite(pPatch->pRPN, 1, pPatch->nRPNSize, f);
}

/*
 * Write a section to a file
 */
static void writesection(struct Section *pSect, FILE *f)
{
	fputstring(pSect->pzName, f);

	fputlong(pSect->nPC, f);

	fputc(pSect->nType, f);

	fputlong(pSect->nOrg, f);
	fputlong(pSect->nBank, f);
	fputlong(pSect->nAlign, f);

	if (sect_HasData(pSect->nType)) {
		struct Patch *pPatch;

		fwrite(pSect->tData, 1, pSect->nPC, f);
		fputlong(countpatches(pSect), f);

		pPatch = pSect->pPatches;
		while (pPatch) {
			writepatch(pPatch, f);
			pPatch = pPatch->pNext;
		}
	}
}

/*
 * Write a symbol to a file
 */
static void writesymbol(struct sSymbol const *pSym, FILE *f)
{
	uint32_t type;
	uint32_t offset;
	int32_t sectid;

	if (!sym_IsDefined(pSym))
		type = SYMTYPE_IMPORT;
	else if (pSym->isExported)
		type = SYMTYPE_EXPORT;
	else
		type = SYMTYPE_LOCAL;

	switch (type) {
	case SYMTYPE_LOCAL:
		offset = pSym->nValue;
		sectid = getsectid(pSym->pSection);
		break;
	case SYMTYPE_IMPORT:
		offset = 0;
		sectid = -1;
		break;
	case SYMTYPE_EXPORT:
		offset = pSym->nValue;
		if (pSym->type != SYM_LABEL)
			sectid = -1;
		else
			sectid = getsectid(pSym->pSection);
		break;
	}

	fputstring(pSym->tzName, f);
	fputc(type, f);

	if (type != SYMTYPE_IMPORT) {
		fputstring(pSym->tzFileName, f);
		fputlong(pSym->nFileLine, f);

		fputlong(sectid, f);
		fputlong(offset, f);
	}
}

/*
 * Add a symbol to the object
 */
static uint32_t nextID;

static uint32_t addsymbol(struct sSymbol const *pSym)
{
	struct PatchSymbol *pPSym, **ppPSym;
	uint32_t hash;

	hash = sym_CalcHash(pSym->tzName);
	ppPSym = &(tHashedPatchSymbols[hash]);

	while ((*ppPSym) != NULL) {
		if (pSym == (*ppPSym)->pSymbol)
			return (*ppPSym)->ID;
		ppPSym = &((*ppPSym)->pBucketNext);
	}

	pPSym = malloc(sizeof(struct PatchSymbol));
	*ppPSym = pPSym;

	if (pPSym == NULL)
		fatalerror("No memory for patchsymbol");

	pPSym->pNext = NULL;
	pPSym->pBucketNext = NULL;
	pPSym->pSymbol = pSym;
	pPSym->ID = nextID++;

	*ppPatchSymbolsTail = pPSym;
	ppPatchSymbolsTail = &(pPSym->pNext);

	return pPSym->ID;
}

/*
 * Add all exported symbols to the object
 */
static void addexports(void)
{
	int32_t i;

	for (i = 0; i < HASHSIZE; i++) {
		struct sSymbol *pSym;

		pSym = tHashedSymbols[i];
		while (pSym) {
			if (pSym->isExported)
				addsymbol(pSym);
			pSym = pSym->pNext;
		}
	}
}

/*
 * Allocate a new patchstructure and link it into the list
 */
struct Patch *allocpatch(void)
{
	struct Patch *pPatch;

	pPatch = malloc(sizeof(struct Patch));

	if (pPatch == NULL)
		fatalerror("No memory for patch");

	pPatch->pNext = pCurrentSection->pPatches;
	pPatch->nRPNSize = 0;
	pPatch->pRPN = NULL;
	pCurrentSection->pPatches = pPatch;

	return pPatch;
}

/*
 * Create a new patch (includes the rpn expr)
 */
void out_CreatePatch(uint32_t type, struct Expression *expr)
{
	struct Patch *pPatch;
	uint16_t rpndata;
	uint8_t *rpnexpr;
	char tzSym[512];
	uint32_t rpnptr = 0, symptr;

	rpnexpr = malloc(expr->nRPNPatchSize);

	if (rpnexpr == NULL)
		fatalerror("No memory for patch RPN expression");

	pPatch = allocpatch();
	pPatch->nType = type;
	fstk_DumpToStr(pPatch->tzFilename, sizeof(pPatch->tzFilename));
	pPatch->nOffset = pCurrentSection->nPC;

	while ((rpndata = rpn_PopByte(expr)) != 0xDEAD) {
		switch (rpndata) {
		case RPN_CONST:
			rpnexpr[rpnptr++] = RPN_CONST;
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			break;
		case RPN_SYM:
		{
			symptr = 0;
			while ((tzSym[symptr++] = rpn_PopByte(expr)) != 0)
				;

			struct sSymbol const *sym = sym_FindSymbol(tzSym);

			if (!sym) {
				break; // TODO: wtf?
			} else if (sym_IsConstant(sym)) {
				uint32_t value;

				value = sym_GetConstantValue(tzSym);
				rpnexpr[rpnptr++] = RPN_CONST;
				rpnexpr[rpnptr++] = value & 0xFF;
				rpnexpr[rpnptr++] = value >> 8;
				rpnexpr[rpnptr++] = value >> 16;
				rpnexpr[rpnptr++] = value >> 24;
			} else {
				symptr = addsymbol(sym);
				rpnexpr[rpnptr++] = RPN_SYM;
				rpnexpr[rpnptr++] = symptr & 0xFF;
				rpnexpr[rpnptr++] = symptr >> 8;
				rpnexpr[rpnptr++] = symptr >> 16;
				rpnexpr[rpnptr++] = symptr >> 24;
			}
			break;
		}
		case RPN_BANK_SYM:
		{
			struct sSymbol *sym;

			symptr = 0;
			while ((tzSym[symptr++] = rpn_PopByte(expr)) != 0)
				;

			sym = sym_FindSymbol(tzSym);
			if (sym == NULL)
				break;

			symptr = addsymbol(sym);
			rpnexpr[rpnptr++] = RPN_BANK_SYM;
			rpnexpr[rpnptr++] = symptr & 0xFF;
			rpnexpr[rpnptr++] = symptr >> 8;
			rpnexpr[rpnptr++] = symptr >> 16;
			rpnexpr[rpnptr++] = symptr >> 24;
			break;
		}
		case RPN_BANK_SECT:
		{
			uint16_t b;

			rpnexpr[rpnptr++] = RPN_BANK_SECT;

			do {
				b = rpn_PopByte(expr);
				rpnexpr[rpnptr++] = b & 0xFF;
			} while (b != 0);
			break;
		}
		default:
			rpnexpr[rpnptr++] = rpndata;
			break;
		}
	}

	assert(rpnptr == expr->nRPNPatchSize);

	pPatch->pRPN = rpnexpr;
	pPatch->nRPNSize = rpnptr;
}

/*
 * Write an objectfile
 */
void out_WriteObject(void)
{
	FILE *f;
	struct PatchSymbol *pSym;
	struct Section *pSect;

	addexports();

	f = fopen(tzObjectname, "wb");
	if (!f)
		err(1, "Couldn't write file '%s'", tzObjectname);

	fprintf(f, RGBDS_OBJECT_VERSION_STRING, RGBDS_OBJECT_VERSION_NUMBER);
	fputlong(RGBDS_OBJECT_REV, f);

	fputlong(countsymbols(), f);
	fputlong(countsections(), f);

	pSym = pPatchSymbols;
	while (pSym) {
		writesymbol(pSym->pSymbol, f);
		pSym = pSym->pNext;
	}

	pSect = pSectionList;
	while (pSect) {
		writesection(pSect, f);
		pSect = pSect->pNext;
	}

	fclose(f);
}

/*
 * Set the objectfilename
 */
void out_SetFileName(char *s)
{
	tzObjectname = s;
	if (CurrentOptions.verbose)
		printf("Output filename %s\n", s);
}
