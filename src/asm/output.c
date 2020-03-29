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
static struct sSymbol *objectSymbols = NULL;
static struct sSymbol **objectSymbolsTail = &objectSymbols;
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

	fputlong(pSect->size, f);

	fputc(pSect->nType | pSect->isUnion << 7, f);

	fputlong(pSect->nOrg, f);
	fputlong(pSect->nBank, f);
	fputlong(pSect->nAlign, f);

	if (sect_HasData(pSect->nType)) {
		struct Patch *pPatch;

		fwrite(pSect->tData, 1, pSect->size, f);
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
	fputstring(pSym->tzName, f);
	if (!sym_IsDefined(pSym)) {
		fputc(SYMTYPE_IMPORT, f);
	} else {
		fputc(pSym->isExported ? SYMTYPE_EXPORT : SYMTYPE_LOCAL, f);
		fputstring(pSym->tzFileName, f);
		fputlong(pSym->nFileLine, f);
		fputlong(pSym->pSection ? getsectid(pSym->pSection) : -1, f);
		fputlong(pSym->nValue, f);
	}
}

/*
 * Returns a symbol's ID within the object file
 * If the symbol does not have one, one is assigned by registering the symbol
 */
static uint32_t getSymbolID(struct sSymbol *sym)
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
			struct sSymbol *sym = sym_FindSymbol(tzSym);
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
			struct sSymbol *sym = sym_FindSymbol(tzSym);
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
	fstk_DumpToStr(pPatch->tzFilename, sizeof(pPatch->tzFilename));
	pPatch->nOffset = ofs;

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
	fputlong(assert->section ? getsectid(assert->section) : -1, f);
	fputstring(assert->message, f);
}

static void registerExportedSymbol(struct sSymbol *symbol, void *arg)
{
	(void)arg;
	if (symbol->isExported && symbol->ID == -1) {
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
	FILE *f;
	struct Section *pSect;
	struct Assertion *assert = assertions;

	f = fopen(tzObjectname, "wb");
	if (!f)
		err(1, "Couldn't write file '%s'", tzObjectname);

	/* Also write exported symbols that weren't written above */
	sym_ForEach(registerExportedSymbol, NULL);

	fprintf(f, RGBDS_OBJECT_VERSION_STRING, RGBDS_OBJECT_VERSION_NUMBER);
	fputlong(RGBDS_OBJECT_REV, f);

	fputlong(nbSymbols, f);
	fputlong(countsections(), f);

	for (struct sSymbol const *sym = objectSymbols; sym; sym = sym->next) {
		writesymbol(sym, f);
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
