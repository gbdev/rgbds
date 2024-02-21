/* SPDX-License-Identifier: MIT */

// Outputs an objectfile

#include <algorithm>
#include <assert.h>
#include <deque>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "asm/charmap.hpp"
#include "asm/fstack.hpp"
#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/rpn.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

#include "error.hpp"
#include "linkdefs.hpp"
#include "platform.hpp" // strdup

struct Patch {
	struct FileStackNode const *src;
	uint32_t lineNo;
	uint32_t offset;
	struct Section *pcSection;
	uint32_t pcOffset;
	uint8_t type;
	uint32_t rpnSize;
	uint8_t *rpn;
};

struct Assertion {
	struct Patch *patch;
	struct Section *section;
	char *message;
};

const char *objectName;

std::deque<struct Section *> sectionList;

// List of symbols to put in the object file
static std::vector<struct Symbol *> objectSymbols;

static std::deque<struct Assertion *> assertions;

static struct FileStackNode *fileStackNodes = NULL;

// Write a long to a file (little-endian)
static void putlong(uint32_t i, FILE *f)
{
	putc(i, f);
	putc(i >> 8, f);
	putc(i >> 16, f);
	putc(i >> 24, f);
}

// Write a NULL-terminated string to a file
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
	// If node is not already registered, register it (and parents), and give it a unique ID
	while (node->ID == (uint32_t)-1) {
		node->ID = getNbFileStackNodes();
		if (node->ID == (uint32_t)-1)
			fatalerror("Reached too many file stack nodes; try splitting the file up\n");
		node->next = fileStackNodes;
		fileStackNodes = node;

		// Also register the node's parents
		node = node->parent;
		if (!node)
			break;
	}
}

void out_ReplaceNode(struct FileStackNode * /* node */)
{
#if 0
This is code intended to replace a node, which is pretty useless until ref counting is added...

	struct FileStackNode **ptr = &fileStackNodes;

	// The linked list is supposed to have decrementing IDs, so iterate with less memory reads,
	// to hopefully hit the cache less. A debug check is added after, in case a change is made
	// that breaks this assumption.
	for (uint32_t i = fileStackNodes->ID; i != node->ID; i--)
		ptr = &(*ptr)->next;
	assert((*ptr)->ID == node->ID);

	node->next = (*ptr)->next;
	assert(!node->next || node->next->ID == node->ID - 1); // Catch inconsistencies early
	// TODO: unreference the node
	*ptr = node;
#endif
}

// Return a section's ID
static uint32_t getsectid(struct Section const *sect)
{
	auto search = std::find(RANGE(sectionList), sect);

	if (search == sectionList.end())
		fatalerror("Unknown section '%s'\n", sect->name);

	return search - sectionList.begin();
}

static uint32_t getSectIDIfAny(struct Section const *sect)
{
	return sect ? getsectid(sect) : (uint32_t)-1;
}

// Write a patch to a file
static void writepatch(struct Patch const *patch, FILE *f)
{
	assert(patch->src->ID != (uint32_t)-1);
	putlong(patch->src->ID, f);
	putlong(patch->lineNo, f);
	putlong(patch->offset, f);
	putlong(getSectIDIfAny(patch->pcSection), f);
	putlong(patch->pcOffset, f);
	putc(patch->type, f);
	putlong(patch->rpnSize, f);
	fwrite(patch->rpn, 1, patch->rpnSize, f);
}

// Write a section to a file
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
		putlong(sect->patches->size(), f);

		for (struct Patch const *patch : *sect->patches)
			writepatch(patch, f);
	}
}

static void freesection(struct Section const *sect)
{
	if (sect_HasData(sect->type)) {
		for (struct Patch *patch : *sect->patches) {
			free(patch->rpn);
			free(patch);
		}
		delete sect->patches;
	}
}

// Write a symbol to a file
static void writesymbol(struct Symbol const *sym, FILE *f)
{
	putstring(sym->name, f);
	if (!sym_IsDefined(sym)) {
		putc(SYMTYPE_IMPORT, f);
	} else {
		assert(sym->src->ID != (uint32_t)-1);

		putc(sym->isExported ? SYMTYPE_EXPORT : SYMTYPE_LOCAL, f);
		putlong(sym->src->ID, f);
		putlong(sym->fileLine, f);
		putlong(getSectIDIfAny(sym_GetSection(sym)), f);
		putlong(sym->value, f);
	}
}

static void registerSymbol(struct Symbol *sym)
{
	sym->ID = objectSymbols.size();
	objectSymbols.push_back(sym);
	out_RegisterNode(sym->src);
}

// Returns a symbol's ID within the object file
// If the symbol does not have one, one is assigned by registering the symbol
static uint32_t getSymbolID(struct Symbol *sym)
{
	if (sym->ID == (uint32_t)-1 && !sym_IsPC(sym))
		registerSymbol(sym);
	return sym->ID;
}

static void writerpn(uint8_t *rpnexpr, uint32_t *rpnptr, const uint8_t *rpn,
		     uint32_t rpnlen)
{
	char symName[512];

	for (size_t offset = 0; offset < rpnlen; ) {
#define popbyte() rpn[offset++]
#define writebyte(byte)	rpnexpr[(*rpnptr)++] = byte
		uint8_t rpndata = popbyte();

		switch (rpndata) {
			struct Symbol *sym;
			uint32_t value;
			uint8_t b;
			size_t i;

		case RPN_CONST:
			writebyte(RPN_CONST);
			writebyte(popbyte());
			writebyte(popbyte());
			writebyte(popbyte());
			writebyte(popbyte());
			break;

		case RPN_SYM:
			i = 0;
			do {
				symName[i] = popbyte();
			} while (symName[i++]);

			// The symbol name is always written expanded
			sym = sym_FindExactSymbol(symName);
			if (sym_IsConstant(sym)) {
				writebyte(RPN_CONST);
				value = sym_GetConstantValue(symName);
			} else {
				writebyte(RPN_SYM);
				value = getSymbolID(sym);
			}

			writebyte(value & 0xFF);
			writebyte(value >> 8);
			writebyte(value >> 16);
			writebyte(value >> 24);
			break;

		case RPN_BANK_SYM:
			i = 0;
			do {
				symName[i] = popbyte();
			} while (symName[i++]);

			// The symbol name is always written expanded
			sym = sym_FindExactSymbol(symName);
			value = getSymbolID(sym);

			writebyte(RPN_BANK_SYM);
			writebyte(value & 0xFF);
			writebyte(value >> 8);
			writebyte(value >> 16);
			writebyte(value >> 24);
			break;

		case RPN_BANK_SECT:
			writebyte(RPN_BANK_SECT);
			do {
				b = popbyte();
				writebyte(b);
			} while (b != 0);
			break;

		case RPN_SIZEOF_SECT:
			writebyte(RPN_SIZEOF_SECT);
			do {
				b = popbyte();
				writebyte(b);
			} while (b != 0);
			break;

		case RPN_STARTOF_SECT:
			writebyte(RPN_STARTOF_SECT);
			do {
				b = popbyte();
				writebyte(b);
			} while (b != 0);
			break;

		default:
			writebyte(rpndata);
			break;
		}
#undef popbyte
#undef writebyte
	}
}

static struct Patch *allocpatch(uint32_t type, struct Expression const *expr, uint32_t ofs)
{
	struct Patch *patch = (struct Patch *)malloc(sizeof(*patch));
	uint32_t rpnSize = rpn_isKnown(expr) ? 5 : expr->rpnPatchSize;
	struct FileStackNode *node = fstk_GetFileStack();

	if (!patch)
		fatalerror("No memory for patch: %s\n", strerror(errno));

	patch->rpn = (uint8_t *)malloc(sizeof(*patch->rpn) * rpnSize);
	if (!patch->rpn)
		fatalerror("No memory for patch's RPN rpnSize: %s\n", strerror(errno));

	patch->type = type;
	patch->src = node;
	// All patches are assumed to eventually be written, so the file stack node is registered
	out_RegisterNode(node);
	patch->lineNo = lexer_GetLineNo();
	patch->offset = ofs;
	patch->pcSection = sect_GetSymbolSection();
	patch->pcOffset = sect_GetSymbolOffset();

	// If the rpnSize's value is known, output a constant RPN rpnSize directly
	if (rpn_isKnown(expr)) {
		patch->rpnSize = rpnSize;
		// Make sure to update `rpnSize` above if modifying this!
		patch->rpn[0] = RPN_CONST;
		patch->rpn[1] = (uint32_t)(expr->val) & 0xFF;
		patch->rpn[2] = (uint32_t)(expr->val) >> 8;
		patch->rpn[3] = (uint32_t)(expr->val) >> 16;
		patch->rpn[4] = (uint32_t)(expr->val) >> 24;
	} else {
		patch->rpnSize = 0;
		writerpn(patch->rpn, &patch->rpnSize, expr->rpn, expr->rpnLength);
	}
	assert(patch->rpnSize == rpnSize);

	return patch;
}

// Create a new patch (includes the rpn expr)
void out_CreatePatch(uint32_t type, struct Expression const *expr, uint32_t ofs, uint32_t pcShift)
{
	struct Patch *patch = allocpatch(type, expr, ofs);

	// If the patch had a quantity of bytes output before it,
	// PC is not at the patch's location, but at the location
	// before those bytes.
	patch->pcOffset -= pcShift;

	// Add the patch to the list
	currentSection->patches->push_front(patch);
}

// Creates an assert that will be written to the object file
bool out_CreateAssert(enum AssertionType type, struct Expression const *expr,
		      char const *message, uint32_t ofs)
{
	struct Assertion *assertion = (struct Assertion *)malloc(sizeof(*assertion));

	if (!assertion)
		return false;

	assertion->patch = allocpatch(type, expr, ofs);
	assertion->message = strdup(message);
	if (!assertion->message) {
		free(assertion);
		return false;
	}

	assertions.push_front(assertion);

	return true;
}

static void writeassert(struct Assertion *assert, FILE *f)
{
	writepatch(assert->patch, f);
	putstring(assert->message, f);
}

static void freeassert(struct Assertion *assert)
{
	free(assert->patch->rpn);
	free(assert->patch);
	free(assert);
}

static void writeFileStackNode(struct FileStackNode const *node, FILE *f)
{
	putlong(node->parent ? node->parent->ID : (uint32_t)-1, f);
	putlong(node->lineNo, f);
	putc(node->type, f);
	if (node->type != NODE_REPT) {
		putstring(((struct FileStackNamedNode const *)node)->name->c_str(), f);
	} else {
		struct FileStackReptNode const *reptNode = (struct FileStackReptNode const *)node;

		putlong(reptNode->iters->size(), f);
		// Iters are stored by decreasing depth, so reverse the order for output
		for (uint32_t i = reptNode->iters->size(); i--; )
			putlong((*reptNode->iters)[i], f);
	}
}

static void registerUnregisteredSymbol(struct Symbol *symbol, void *)
{
	// Check for symbol->src, to skip any built-in symbol from rgbasm
	if (symbol->src && symbol->ID == (uint32_t)-1) {
		registerSymbol(symbol);
	}
}

// Write an objectfile
void out_WriteObject(void)
{
	FILE *f;

	if (strcmp(objectName, "-")) {
		f = fopen(objectName, "wb");
	} else {
		objectName = "<stdout>";
		f = fdopen(STDOUT_FILENO, "wb");
	}
	if (!f)
		err("Failed to open object file '%s'", objectName);

	// Also write symbols that weren't written above
	sym_ForEach(registerUnregisteredSymbol, NULL);

	fprintf(f, RGBDS_OBJECT_VERSION_STRING);
	putlong(RGBDS_OBJECT_REV, f);

	putlong(objectSymbols.size(), f);
	putlong(sectionList.size(), f);

	putlong(getNbFileStackNodes(), f);
	for (struct FileStackNode const *node = fileStackNodes; node; node = node->next) {
		writeFileStackNode(node, f);
		if (node->next && node->next->ID != node->ID - 1)
			fatalerror("Internal error: fstack node #%" PRIu32 " follows #%" PRIu32
				   ". Please report this to the developers!\n",
				   node->next->ID, node->ID);
	}

	for (struct Symbol const *sym : objectSymbols)
		writesymbol(sym, f);

	for (struct Section *sect : sectionList) {
		writesection(sect, f);
		freesection(sect);
	}

	putlong(assertions.size(), f);

	for (struct Assertion *assert : assertions) {
		writeassert(assert, f);
		freeassert(assert);
	}

	fclose(f);
}

// Set the objectfilename
void out_SetFileName(char *s)
{
	if (objectName)
		warnx("Overriding output filename %s", objectName);
	objectName = s;
	if (verbose)
		printf("Output filename %s\n", objectName);
}
