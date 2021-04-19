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
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
	struct FileStackNode const *src;
	uint32_t lineNo;
	uint32_t offset;
	struct Section *pcSection;
	uint32_t pcOffset;
	uint8_t type;
	struct RPNBuffer *rpn;
	struct Patch *next;
};

struct Assertion {
	struct Patch *patch;
	struct Section *section;
	char *message;
	struct Assertion *next;
};

char *objectName;

/* TODO: shouldn't `currentSection` be somewhere else? */
struct Section *sectionList, *currentSection;

/* Linked list of symbols to put in the object file */
static struct Symbol *objectSymbols = NULL;
static struct Symbol **objectSymbolsTail = &objectSymbols;
static uint32_t nbSymbols = 0; /* Length of the above list */

static struct Assertion *assertions = NULL;

static struct FileStackNode *fileStackNodes = NULL;

/*
 * Count the number of sections used in this object
 */
static uint32_t countSections(void)
{
	uint32_t count = 0;

	for (struct Section const *sect = sectionList; sect; sect = sect->next)
		count++;

	return count;
}

/*
 * Count the number of patches used in this object
 */
static uint32_t countPatches(struct Section const *sect)
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
static uint32_t countAsserts(void)
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
static void putlong(uint32_t i, FILE *f)
{
	putc(i, f);
	putc(i >> 8, f);
	putc(i >> 16, f);
	putc(i >> 24, f);
}

/*
 * Write a NULL-terminated string to a file
 */
static void putstring(char const *s, FILE *f)
{
	while (*s)
		putc(*s++, f);
	putc(0, f);
}

static uint32_t getNbFileStackNodes(void)
{
	return fileStackNodes ? fileStackNodes->ID + 1 : 0;
}

void out_RegisterNode(struct FileStackNode *node)
{
	/* If node is not already registered, register it (and parents), and give it a unique ID */
	while (node->ID == (uint32_t)-1) {
		node->ID = getNbFileStackNodes();
		if (node->ID == (uint32_t)-1)
			fatalerror("Reached too many file stack nodes; try splitting the file up\n");
		node->next = fileStackNodes;
		fileStackNodes = node;

		/* Also register the node's parents */
		node = node->parent;
		if (!node)
			break;
	}
}

void out_ReplaceNode(struct FileStackNode *node)
{
	(void)node;
#if 0
This is code intended to replace a node, which is pretty useless until ref counting is added...

	struct FileStackNode **ptr = &fileStackNodes;

	/*
	 * The linked list is supposed to have decrementing IDs, so iterate with less memory reads,
	 * to hopefully hit the cache less. A debug check is added after, in case a change is made
	 * that breaks this assumption.
	 */
	for (uint32_t i = fileStackNodes->ID; i != node->ID; i--)
		ptr = &(*ptr)->next;
	assert((*ptr)->ID == node->ID);

	node->next = (*ptr)->next;
	assert(!node->next || node->next->ID == node->ID - 1); /* Catch inconsistencies early */
	/* TODO: unreference the node */
	*ptr = node;
#endif
}

/*
 * Return a section's ID
 */
static uint32_t getsectid(struct Section const *sect)
{
	struct Section const *sec = sectionList;
	uint32_t ID = 0;

	while (sec) {
		if (sec == sect)
			return ID;
		ID++;
		sec = sec->next;
	}

	fatalerror("Unknown section '%s'\n", sect->name);
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
	assert(patch->src->ID != -1);
	putlong(patch->src->ID, f);
	putlong(patch->lineNo, f);
	putlong(patch->offset, f);
	putlong(getSectIDIfAny(patch->pcSection), f);
	putlong(patch->pcOffset, f);
	putc(patch->type, f);
	putlong(patch->rpn->size, f);
	fwrite(patch->rpn->buf, 1, patch->rpn->size, f);
}

/*
 * Write a section to a file
 */
static void writesection(struct Section const *sect, FILE *f)
{
	putstring(sect->name, f);

	putlong(sect->size, f);

	bool isUnion = sect->modifier == SECTION_UNION;
	bool isFragment = sect->modifier == SECTION_FRAGMENT;

	putc(sect->type | isUnion << 7 | isFragment << 6, f);

	putlong(sect->org, f);
	putlong(sect->bank, f);
	putc(sect->align, f);
	putlong(sect->alignOfs, f);

	if (sect_HasData(sect->type)) {
		fwrite(sect->data, 1, sect->size, f);
		putlong(countPatches(sect), f);
		struct Patch *patch = sect->patches;

		while (patch != NULL) {
			struct Patch *next = patch->next;

			writepatch(patch, f);
			free(patch->rpn);
			free(patch);
			patch = next;
		}
	}
}

/*
 * Write a symbol to a file
 */
static void writesymbol(struct Symbol const *sym, FILE *f)
{
	putstring(sym->name, f);
	if (!sym_IsDefined(sym)) {
		putc(SYMTYPE_IMPORT, f);
	} else {
		assert(sym->src->ID != -1);

		putc(sym->isExported ? SYMTYPE_EXPORT : SYMTYPE_LOCAL, f);
		putlong(sym->src->ID, f);
		putlong(sym->fileLine, f);
		putlong(getSectIDIfAny(sym_GetSection(sym)), f);
		putlong(sym->value, f);
	}
}

static void registerSymbol(struct Symbol *sym)
{
	*objectSymbolsTail = sym;
	objectSymbolsTail = &sym->next;
	out_RegisterNode(sym->src);
	if (nbSymbols == (uint32_t)-1)
		fatalerror("Registered too many symbols (%" PRIu32
			   "); try splitting up your files\n", (uint32_t)-1);
	sym->ID = nbSymbols++;
}

/*
 * Returns a symbol's ID within the object file
 * If the symbol does not have one, one is assigned by registering the symbol
 */
uint32_t out_GetSymbolID(struct Symbol *sym)
{
	if (sym->ID == (uint32_t)-1 && !sym_IsPC(sym))
		registerSymbol(sym);
	return sym->ID;
}

/*
 * Allocate a new patch structure and link it into the list
 * WARNING: all patches are assumed to eventually be written, so the file stack node is registered
 */
static struct Patch *allocpatch(uint32_t type, struct RPNBuffer *rpn, uint32_t ofs)
{
	struct Patch *patch = malloc(sizeof(struct Patch));
	struct FileStackNode *node = fstk_GetFileStack();

	if (!patch)
		fatalerror("No memory for patch: %s\n", strerror(errno));

	patch->type = type;
	patch->src = node;
	out_RegisterNode(node);
	patch->lineNo = lexer_GetLineNo();
	patch->offset = ofs;
	patch->pcSection = sect_GetSymbolSection();
	patch->pcOffset = sect_GetSymbolOffset();
	patch->rpn = rpn;

	return patch;
}

/*
 * Create a new patch (includes the rpn expr)
 */
void out_CreatePatch(uint32_t type, struct RPNBuffer *rpn, uint32_t ofs, uint32_t pcShift)
{
	struct Patch *patch = allocpatch(type, rpn, ofs);

	// If the patch had a quantity of bytes output before it,
	// PC is not at the patch's location, but at the location
	// before those bytes.
	patch->pcOffset -= pcShift;

	patch->next = currentSection->patches;
	currentSection->patches = patch;
}

/**
 * Creates an assert that will be written to the object file
 */
bool out_CreateAssert(enum AssertionType type, struct RPNBuffer *rpn,
		      char const *message, uint32_t ofs)
{
	struct Assertion *assertion = malloc(sizeof(*assertion));

	if (!assertion)
		return false;

	assertion->patch = allocpatch(type, rpn, ofs);
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
	putstring(assert->message, f);
}

static void writeFileStackNode(struct FileStackNode const *node, FILE *f)
{
	putlong(node->parent ? node->parent->ID : -1, f);
	putlong(node->lineNo, f);
	putc(node->type, f);
	if (node->type != NODE_REPT) {
		putstring(((struct FileStackNamedNode const *)node)->name, f);
	} else {
		struct FileStackReptNode const *reptNode = (struct FileStackReptNode const *)node;

		putlong(reptNode->reptDepth, f);
		/* Iters are stored by decreasing depth, so reverse the order for output */
		for (uint32_t i = reptNode->reptDepth; i--; )
			putlong(reptNode->iters[i], f);
	}
}

static void registerUnregisteredSymbol(struct Symbol *symbol, void *arg)
{
	(void)arg; // sym_ForEach requires a void* parameter, but we are not using it.

	// Check for symbol->src, to skip any built-in symbol from rgbasm
	if (symbol->src && symbol->ID == (uint32_t)-1) {
		registerSymbol(symbol);
	}
}

/*
 * Write an objectfile
 */
void out_WriteObject(void)
{
	FILE *f;
	if (strcmp(objectName, "-") != 0)
		f = fopen(objectName, "wb");
	else
		f = fdopen(1, "wb");

	if (!f)
		err(1, "Couldn't write file '%s'", objectName);

	/* Also write symbols that weren't written above */
	sym_ForEach(registerUnregisteredSymbol, NULL);

	fprintf(f, RGBDS_OBJECT_VERSION_STRING, RGBDS_OBJECT_VERSION_NUMBER);
	putlong(RGBDS_OBJECT_REV, f);

	putlong(nbSymbols, f);
	putlong(countSections(), f);

	putlong(getNbFileStackNodes(), f);
	for (struct FileStackNode const *node = fileStackNodes; node; node = node->next) {
		writeFileStackNode(node, f);
		if (node->next && node->next->ID != node->ID - 1)
			fatalerror("Internal error: fstack node #%" PRIu32 " follows #%" PRIu32
				   ". Please report this to the developers!\n",
				   node->next->ID, node->ID);
	}

	for (struct Symbol const *sym = objectSymbols; sym; sym = sym->next)
		writesymbol(sym, f);

	for (struct Section *sect = sectionList; sect; sect = sect->next)
		writesection(sect, f);

	putlong(countAsserts(), f);
	struct Assertion *assert = assertions;
	while (assert) {
		struct Assertion *next = assert->next;

		writeassert(assert, f);
		free(assert->patch->rpn);
		free(assert->patch);
		free(assert);
		assert = next;
	}

	fclose(f);
}

/*
 * Set the objectfilename
 */
void out_SetFileName(char *s)
{
	objectName = s;
	if (verbose)
		printf("Output filename %s\n", s);
}
