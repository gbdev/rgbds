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

struct Assertion {
	struct Patch *patch;
	struct Section *section;
	char *message;
	struct Assertion *next;
};

struct PatchSymbol *tHashedPatchSymbols[HASHSIZE];
struct Section *pSectionList, *pCurrentSection;
struct PatchSymbol *pPatchSymbols;
struct PatchSymbol **ppPatchSymbolsTail = &pPatchSymbols;
struct Assertion *assertions = NULL;
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

/**
 * Count the number of assertions used in this object
 */
static uint32_t countasserts(void)
{
	struct Assertion *assert = assertions;
	uint32_t count = 0;

	while (assert) {
		count++;
		assert = assert->next;
	}
	return count;
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

static void writerpn(uint8_t *rpnexpr, uint32_t *rpnptr, uint8_t *rpn,
		     uint32_t rpnlen)
{
	char tzSym[512];

	for (size_t offset = 0; offset < rpnlen; ) {
#define popbyte() rpn[offset++]
#define writebyte(byte)	rpnexpr[(*rpnptr)++] = byte
		uint8_t rpndata = popbyte();

		switch (rpndata) {
		case RPN_CONST:
			writebyte(RPN_CONST);
			writebyte(popbyte());
			writebyte(popbyte());
			writebyte(popbyte());
			writebyte(popbyte());
			break;
		case RPN_SYM:
		{
			uint32_t symptr = 0;

			while ((tzSym[symptr++] = popbyte()) != 0)
				;

			struct sSymbol const *sym = sym_FindSymbol(tzSym);

			if (!sym) {
				break; // TODO: wtf?
			} else if (sym_IsConstant(sym)) {
				uint32_t value;

				value = sym_GetConstantValue(tzSym);
				writebyte(RPN_CONST);
				writebyte(value & 0xFF);
				writebyte(value >> 8);
				writebyte(value >> 16);
				writebyte(value >> 24);
			} else {
				symptr = addsymbol(sym);
				writebyte(RPN_SYM);
				writebyte(symptr & 0xFF);
				writebyte(symptr >> 8);
				writebyte(symptr >> 16);
				writebyte(symptr >> 24);
			}
			break;
		}
		case RPN_BANK_SYM:
		{
			struct sSymbol *sym;
			uint32_t symptr = 0;

			while ((tzSym[symptr++] = popbyte()) != 0)
				;

			sym = sym_FindSymbol(tzSym);
			if (sym == NULL)
				break;

			symptr = addsymbol(sym);
			writebyte(RPN_BANK_SYM);
			writebyte(symptr & 0xFF);
			writebyte(symptr >> 8);
			writebyte(symptr >> 16);
			writebyte(symptr >> 24);
			break;
		}
		case RPN_BANK_SECT:
		{
			uint16_t b;

			writebyte(RPN_BANK_SECT);

			do {
				b = popbyte();
				writebyte(b & 0xFF);
			} while (b != 0);
			break;
		}
		default:
			writebyte(rpndata);
			break;
		}
#undef popbyte
#undef writebyte
	}
}

/*
 * Allocate a new patch structure and link it into the list
 */
static struct Patch *allocpatch(uint32_t type, struct Expression const *expr)
{
	struct Patch *pPatch;

	pPatch = malloc(sizeof(struct Patch));

	if (!pPatch)
		fatalerror("No memory for patch: %s", strerror(errno));
	pPatch->pRPN = malloc(sizeof(*pPatch->pRPN) * expr->nRPNPatchSize);

	if (!pPatch->pRPN)
		fatalerror("No memory for patch's RPN expression: %s",
			   strerror(errno));

	pPatch->nRPNSize = 0;
	pPatch->nType = type;
	pPatch->nOffset = pCurrentSection->nPC;
	fstk_DumpToStr(pPatch->tzFilename, sizeof(pPatch->tzFilename));

	writerpn(pPatch->pRPN, &pPatch->nRPNSize, expr->tRPN, expr->nRPNLength);
	assert(pPatch->nRPNSize == expr->nRPNPatchSize);

	return pPatch;
}

/*
 * Create a new patch (includes the rpn expr)
 */
void out_CreatePatch(uint32_t type, struct Expression const *expr)
{
	struct Patch *pPatch = allocpatch(type, expr);

	pPatch->pNext = pCurrentSection->pPatches;
	pCurrentSection->pPatches = pPatch;
}

/**
 * Creates an assert that will be written to the object file
 */
bool out_CreateAssert(enum AssertionType type, struct Expression const *expr,
		      char const *message)
{
	struct Assertion *assertion = malloc(sizeof(*assertion));

	if (!assertion)
		return false;

	assertion->patch = allocpatch(type, expr);
	assertion->section = pCurrentSection;
	assertion->message = strdup(message);
	if (!assertion->message) {
		free(assertion);
		return false;
	}

	assertion->next = assertions;
	assertions = assertion;

	return true;
}

static void writeassert(struct Assertion *assert, FILE *f)
{
	writepatch(assert->patch, f);
	fputlong(getsectid(assert->section), f);
	fputstring(assert->message, f);
}

/*
 * Write an objectfile
 */
void out_WriteObject(void)
{
	FILE *f;
	struct PatchSymbol *pSym;
	struct Section *pSect;
	struct Assertion *assert = assertions;

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

	fputlong(countasserts(), f);
	while (assert) {
		writeassert(assert, f);
		assert = assert->next;
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
