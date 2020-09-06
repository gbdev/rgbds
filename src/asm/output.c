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
	uint8_t type;
	uint32_t nRPNSize;
	uint8_t *pRPN;
	struct Patch *next;
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
	struct Section *sect;
	uint32_t count = 0;

	sect = pSectionList;
	while (sect) {
		count++;
		sect = sect->next;
	}

	return count;
}

/*
 * Count the number of patches used in this object
 */
static uint32_t countpatches(struct Section const *sect)
{
	uint32_t r = 0;

	for (struct Patch const *patch = sect->patches; patch != NULL;
	     patch = patch->next)
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
static uint32_t getsectid(struct Section const *sect)
{
	struct Section const *sec;
	uint32_t ID = 0;

	sec = pSectionList;

	while (sec) {
		if (sec == sect)
			return ID;
		ID++;
		sec = sec->next;
	}

	fatalerror("Unknown section '%s'", sect->name);
}

static uint32_t getSectIDIfAny(struct Section const *sect)
{
	return sect ? getsectid(sect) : -1;
}

/*
 * Write a patch to a file
 */
static void writepatch(struct Patch const *patch, FILE *f)
{
	fputstring(patch->tzFilename, f);
	fputlong(patch->nOffset, f);
	fputlong(getSectIDIfAny(patch->pcSection), f);
	fputlong(patch->pcOffset, f);
	fputc(patch->type, f);
	fputlong(patch->nRPNSize, f);
	fwrite(patch->pRPN, 1, patch->nRPNSize, f);
}

/*
 * Write a section to a file
 */
static void writesection(struct Section const *sect, FILE *f)
{
	fputstring(sect->name, f);

	fputlong(sect->size, f);

	bool isUnion = sect->modifier == SECTION_UNION;
	bool isFragment = sect->modifier == SECTION_FRAGMENT;

	fputc(sect->type | isUnion << 7 | isFragment << 6, f);

	fputlong(sect->org, f);
	fputlong(sect->bank, f);
	fputc(sect->align, f);
	fputlong(sect->alignOfs, f);

	if (sect_HasData(sect->type)) {
		fwrite(sect->data, 1, sect->size, f);
		fputlong(countpatches(sect), f);

		for (struct Patch const *patch = sect->patches; patch != NULL;
		     patch = patch->next)
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
	struct Patch *patch = malloc(sizeof(struct Patch));
	uint32_t rpnSize = expr->isKnown ? 5 : expr->nRPNPatchSize;

	if (!patch)
		fatalerror("No memory for patch: %s", strerror(errno));
	patch->pRPN = malloc(sizeof(*patch->pRPN) * rpnSize);

	if (!patch->pRPN)
		fatalerror("No memory for patch's RPN expression: %s",
			   strerror(errno));

	patch->type = type;
	fstk_DumpToStr(patch->tzFilename, sizeof(patch->tzFilename));
	patch->nOffset = ofs;
	patch->pcSection = sect_GetSymbolSection();
	patch->pcOffset = sect_GetSymbolOffset();

	/* If the expression's value is known, output a constant RPN expression directly */
	if (expr->isKnown) {
		patch->nRPNSize = rpnSize;
		/* Make sure to update `rpnSize` above if modifying this! */
		patch->pRPN[0] = RPN_CONST;
		patch->pRPN[1] = (uint32_t)(expr->nVal) & 0xFF;
		patch->pRPN[2] = (uint32_t)(expr->nVal) >> 8;
		patch->pRPN[3] = (uint32_t)(expr->nVal) >> 16;
		patch->pRPN[4] = (uint32_t)(expr->nVal) >> 24;
	} else {
		patch->nRPNSize = 0;
		writerpn(patch->pRPN, &patch->nRPNSize, expr->tRPN, expr->nRPNLength);
	}
	assert(patch->nRPNSize == rpnSize);

	return patch;
}

/*
 * Create a new patch (includes the rpn expr)
 */
void out_CreatePatch(uint32_t type, struct Expression const *expr, uint32_t ofs)
{
	struct Patch *patch = allocpatch(type, expr, ofs);

	patch->next = pCurrentSection->patches;
	pCurrentSection->patches = patch;
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

	for (struct Section *sect = pSectionList; sect; sect = sect->next)
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
