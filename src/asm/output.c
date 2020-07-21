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
#include "platform.h" // strdup

struct Patch {
	char tzFilename[_MAX_PATH + 1];
	uint32_t nOffset;
	struct Section *pcSection;
	uint32_t pcOffset;
	uint8_t nType;
	uint32_t nRPNSize;
	uint8_t *pRPN;
	struct Patch *pNext;
};

struct Assertion {
	struct Patch *patch;
	struct Section *section;
	char *message;
	struct Assertion *next;
};

char *tzObjectname;

/* TODO: shouldn't `pCurrentSection` be somewhere else? */
struct Section *pSectionList, *pCurrentSection;

/* Linked list of symbols to put in the object file */
static struct Symbol *objectSymbols = NULL;
static struct Symbol **objectSymbolsTail = &objectSymbols;
static uint32_t nbSymbols = 0; /* Length of the above list */

static struct Assertion *assertions = NULL;

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
static uint32_t countpatches(struct Section const *pSect)
{
	uint32_t r = 0;

	for (struct Patch const *patch = pSect->pPatches; patch != NULL;
	     patch = patch->pNext)
		r++;

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
static uint32_t getsectid(struct Section const *pSect)
{
	struct Section const *sec;
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

static uint32_t getSectIDIfAny(struct Section const *sect)
{
	return sect ? getsectid(sect) : -1;
}

/*
 * Write a patch to a file
 */
static void writepatch(struct Patch const *pPatch, FILE *f)
{
	fputstring(pPatch->tzFilename, f);
	fputlong(pPatch->nOffset, f);
	fputlong(getSectIDIfAny(pPatch->pcSection), f);
	fputlong(pPatch->pcOffset, f);
	fputc(pPatch->nType, f);
	fputlong(pPatch->nRPNSize, f);
	fwrite(pPatch->pRPN, 1, pPatch->nRPNSize, f);
}

/*
 * Write a section to a file
 */
static void writesection(struct Section const *pSect, FILE *f)
{
	fputstring(pSect->pzName, f);

	fputlong(pSect->size, f);

	bool isUnion = pSect->modifier == SECTION_UNION;
	bool isFragment = pSect->modifier == SECTION_FRAGMENT;

	fputc(pSect->nType | isUnion << 7 | isFragment << 6, f);

	fputlong(pSect->nOrg, f);
	fputlong(pSect->nBank, f);
	fputc(pSect->nAlign, f);
	fputlong(pSect->alignOfs, f);

	if (sect_HasData(pSect->nType)) {
		fwrite(pSect->tData, 1, pSect->size, f);
		fputlong(countpatches(pSect), f);

		for (struct Patch const *patch = pSect->pPatches; patch != NULL;
		     patch = patch->pNext)
			writepatch(patch, f);
	}
}

/*
 * Write a symbol to a file
 */
static void writesymbol(struct Symbol const *sym, FILE *f)
{
	fputstring(sym->name, f);
	if (!sym_IsDefined(sym)) {
		fputc(SYMTYPE_IMPORT, f);
	} else {
		fputc(sym->isExported ? SYMTYPE_EXPORT : SYMTYPE_LOCAL, f);
		fputstring(sym->fileName, f);
		fputlong(sym->fileLine, f);
		fputlong(getSectIDIfAny(sym_GetSection(sym)), f);
		fputlong(sym->value, f);
	}
}

/*
 * Returns a symbol's ID within the object file
 * If the symbol does not have one, one is assigned by registering the symbol
 */
static uint32_t getSymbolID(struct Symbol *sym)
{
	if (sym->ID == -1) {
		sym->ID = nbSymbols++;

		*objectSymbolsTail = sym;
		objectSymbolsTail = &sym->next;
	}
	return sym->ID;
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
			for (unsigned int i = -1; (tzSym[++i] = popbyte()); )
				;
			struct Symbol *sym = sym_FindSymbol(tzSym);
			uint32_t value;

			if (sym_IsConstant(sym)) {
				writebyte(RPN_CONST);
				value = sym_GetConstantValue(tzSym);
			} else {
				writebyte(RPN_SYM);
				value = getSymbolID(sym);
			}
			writebyte(value & 0xFF);
			writebyte(value >> 8);
			writebyte(value >> 16);
			writebyte(value >> 24);
			break;
		}
		case RPN_BANK_SYM:
		{
			for (unsigned int i = -1; (tzSym[++i] = popbyte()); )
				;
			struct Symbol *sym = sym_FindSymbol(tzSym);
			uint32_t value = getSymbolID(sym);

			writebyte(RPN_BANK_SYM);
			writebyte(value & 0xFF);
			writebyte(value >> 8);
			writebyte(value >> 16);
			writebyte(value >> 24);
			break;
		}
		case RPN_BANK_SECT:
		{
			uint8_t b;

			writebyte(RPN_BANK_SECT);
			do {
				b = popbyte();
				writebyte(b);
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
static struct Patch *allocpatch(uint32_t type, struct Expression const *expr,
				uint32_t ofs)
{
	struct Patch *pPatch = malloc(sizeof(struct Patch));

	if (!pPatch)
		fatalerror("No memory for patch: %s", strerror(errno));
	pPatch->pRPN = malloc(sizeof(*pPatch->pRPN) * expr->nRPNPatchSize);

	if (!pPatch->pRPN)
		fatalerror("No memory for patch's RPN expression: %s",
			   strerror(errno));

	pPatch->nRPNSize = 0;
	pPatch->nType = type;
	fstk_DumpToStr(pPatch->tzFilename, sizeof(pPatch->tzFilename));
	pPatch->nOffset = ofs;
	pPatch->pcSection = sect_GetSymbolSection();
	pPatch->pcOffset = curOffset;

	writerpn(pPatch->pRPN, &pPatch->nRPNSize, expr->tRPN, expr->nRPNLength);
	assert(pPatch->nRPNSize == expr->nRPNPatchSize);

	return pPatch;
}

/*
 * Create a new patch (includes the rpn expr)
 */
void out_CreatePatch(uint32_t type, struct Expression const *expr, uint32_t ofs)
{
	struct Patch *pPatch = allocpatch(type, expr, ofs);

	pPatch->pNext = pCurrentSection->pPatches;
	pCurrentSection->pPatches = pPatch;
}

/**
 * Creates an assert that will be written to the object file
 */
bool out_CreateAssert(enum AssertionType type, struct Expression const *expr,
		      char const *message, uint32_t ofs)
{
	struct Assertion *assertion = malloc(sizeof(*assertion));

	if (!assertion)
		return false;

	assertion->patch = allocpatch(type, expr, ofs);
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
	fputstring(assert->message, f);
}

static void registerExportedSymbol(struct Symbol *symbol, void *arg)
{
	(void)arg;
	if (sym_IsExported(symbol) && symbol->ID == -1) {
		*objectSymbolsTail = symbol;
		objectSymbolsTail = &symbol->next;
		nbSymbols++;
	}
}

/*
 * Write an objectfile
 */
void out_WriteObject(void)
{
	FILE *f = fopen(tzObjectname, "wb");

	if (!f)
		err(1, "Couldn't write file '%s'", tzObjectname);

	/* Also write exported symbols that weren't written above */
	sym_ForEach(registerExportedSymbol, NULL);

	fprintf(f, RGBDS_OBJECT_VERSION_STRING, RGBDS_OBJECT_VERSION_NUMBER);
	fputlong(RGBDS_OBJECT_REV, f);

	fputlong(nbSymbols, f);
	fputlong(countsections(), f);

	for (struct Symbol const *sym = objectSymbols; sym; sym = sym->next)
		writesymbol(sym, f);

	for (struct Section *sect = pSectionList; sect; sect = sect->pNext)
		writesection(sect, f);

	fputlong(countasserts(), f);
	for (struct Assertion *assert = assertions; assert;
	     assert = assert->next)
		writeassert(assert, f);

	fclose(f);
}

/*
 * Set the objectfilename
 */
void out_SetFileName(char *s)
{
	tzObjectname = s;
	if (verbose)
		printf("Output filename %s\n", s);
}
