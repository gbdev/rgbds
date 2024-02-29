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
#include <string>
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

struct Assertion {
	Patch patch;
	Section *section;
	std::string message;
};

const char *objectName;

// List of symbols to put in the object file
static std::vector<Symbol *> objectSymbols;

static std::deque<Assertion> assertions;

static std::deque<FileStackNode *> fileStackNodes;

// Write a long to a file (little-endian)
static void putlong(uint32_t i, FILE *f)
{
	putc(i, f);
	putc(i >> 8, f);
	putc(i >> 16, f);
	putc(i >> 24, f);
}

// Write a NUL-terminated string to a file
static void putstring(char const *s, FILE *f)
{
	while (*s)
		putc(*s++, f);
	putc(0, f);
}

void out_RegisterNode(FileStackNode *node)
{
	// If node is not already registered, register it (and parents), and give it a unique ID
	for (; node && node->ID == (uint32_t)-1; node = node->parent) {
		node->ID = fileStackNodes.size();
		fileStackNodes.push_front(node);
	}
}

void out_ReplaceNode(FileStackNode * /* node */)
{
#if 0
This is code intended to replace a node, which is pretty useless until ref counting is added...

	auto search = std::find(RANGE(fileStackNodes), node);
	assert(search != fileStackNodes.end());
	// The list is supposed to have decrementing IDs; catch inconsistencies early
	assert(search->ID == node->ID);
	assert(search + 1 == fileStackNodes.end() || (search + 1)->ID == node->ID - 1);

	// TODO: unreference the node
	*search = node;
#endif
}

// Return a section's ID, or -1 if the section is not in the list
static uint32_t getSectIDIfAny(Section *sect)
{
	if (!sect)
		return (uint32_t)-1;

	for (auto it = sectionList.begin(); it != sectionList.end(); it++) {
		if (&*it == sect)
			return it - sectionList.begin();
	}

	fatalerror("Unknown section '%s'\n", sect->name);
}

// Write a patch to a file
static void writepatch(Patch const &patch, FILE *f)
{
	assert(patch.src->ID != (uint32_t)-1);
	putlong(patch.src->ID, f);
	putlong(patch.lineNo, f);
	putlong(patch.offset, f);
	putlong(getSectIDIfAny(patch.pcSection), f);
	putlong(patch.pcOffset, f);
	putc(patch.type, f);
	putlong(patch.rpn.size(), f);
	fwrite(patch.rpn.data(), 1, patch.rpn.size(), f);
}

// Write a section to a file
static void writesection(Section const &sect, FILE *f)
{
	putstring(sect.name, f);

	putlong(sect.size, f);

	bool isUnion = sect.modifier == SECTION_UNION;
	bool isFragment = sect.modifier == SECTION_FRAGMENT;

	putc(sect.type | isUnion << 7 | isFragment << 6, f);

	putlong(sect.org, f);
	putlong(sect.bank, f);
	putc(sect.align, f);
	putlong(sect.alignOfs, f);

	if (sect_HasData(sect.type)) {
		fwrite(sect.data.data(), 1, sect.size, f);
		putlong(sect.patches.size(), f);

		for (Patch const &patch : sect.patches)
			writepatch(patch, f);
	}
}

// Write a symbol to a file
static void writesymbol(Symbol const *sym, FILE *f)
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

static void registerSymbol(Symbol *sym)
{
	sym->ID = objectSymbols.size();
	objectSymbols.push_back(sym);
	out_RegisterNode(sym->src);
}

// Returns a symbol's ID within the object file
// If the symbol does not have one, one is assigned by registering the symbol
static uint32_t getSymbolID(Symbol *sym)
{
	if (sym->ID == (uint32_t)-1 && !sym_IsPC(sym))
		registerSymbol(sym);
	return sym->ID;
}

static void writerpn(std::vector<uint8_t> &rpnexpr, const std::vector<uint8_t> &rpn)
{
	char symName[512];
	size_t rpnptr = 0;

	for (size_t offset = 0; offset < rpn.size(); ) {
#define popbyte() rpn[offset++]
#define writebyte(byte)	rpnexpr[rpnptr++] = byte
		uint8_t rpndata = popbyte();

		switch (rpndata) {
			Symbol *sym;
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

static void initpatch(Patch &patch, uint32_t type, Expression const *expr, uint32_t ofs)
{
	FileStackNode *node = fstk_GetFileStack();

	patch.type = type;
	patch.src = node;
	// All patches are assumed to eventually be written, so the file stack node is registered
	out_RegisterNode(node);
	patch.lineNo = lexer_GetLineNo();
	patch.offset = ofs;
	patch.pcSection = sect_GetSymbolSection();
	patch.pcOffset = sect_GetSymbolOffset();

	if (rpn_isKnown(expr)) {
		// If the RPN expr's value is known, output a constant directly
		patch.rpn.resize(5);
		patch.rpn[0] = RPN_CONST;
		patch.rpn[1] = (uint32_t)(expr->val) & 0xFF;
		patch.rpn[2] = (uint32_t)(expr->val) >> 8;
		patch.rpn[3] = (uint32_t)(expr->val) >> 16;
		patch.rpn[4] = (uint32_t)(expr->val) >> 24;
	} else {
		patch.rpn.resize(expr->rpnPatchSize);
		writerpn(patch.rpn, *expr->rpn);
	}
}

// Create a new patch (includes the rpn expr)
void out_CreatePatch(uint32_t type, Expression const *expr, uint32_t ofs, uint32_t pcShift)
{
	// Add the patch to the list
	Patch &patch = currentSection->patches.emplace_front();

	initpatch(patch, type, expr, ofs);

	// If the patch had a quantity of bytes output before it,
	// PC is not at the patch's location, but at the location
	// before those bytes.
	patch.pcOffset -= pcShift;
}

// Creates an assert that will be written to the object file
void out_CreateAssert(enum AssertionType type, Expression const *expr, char const *message, uint32_t ofs)
{
	Assertion &assertion = assertions.emplace_front();

	initpatch(assertion.patch, type, expr, ofs);
	assertion.message = message;
}

static void writeassert(Assertion &assert, FILE *f)
{
	writepatch(assert.patch, f);
	putstring(assert.message.c_str(), f);
}

static void writeFileStackNode(FileStackNode const *node, FILE *f)
{
	putlong(node->parent ? node->parent->ID : (uint32_t)-1, f);
	putlong(node->lineNo, f);
	putc(node->type, f);
	if (node->type != NODE_REPT) {
		putstring(node->name().c_str(), f);
	} else {
		std::vector<uint32_t> const &nodeIters = node->iters();

		putlong(nodeIters.size(), f);
		// Iters are stored by decreasing depth, so reverse the order for output
		for (uint32_t i = nodeIters.size(); i--; )
			putlong(nodeIters[i], f);
	}
}

static void registerUnregisteredSymbol(Symbol *symbol)
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
	sym_ForEach(registerUnregisteredSymbol);

	fprintf(f, RGBDS_OBJECT_VERSION_STRING);
	putlong(RGBDS_OBJECT_REV, f);

	putlong(objectSymbols.size(), f);
	putlong(sectionList.size(), f);

	putlong(fileStackNodes.size(), f);
	for (auto it = fileStackNodes.begin(); it != fileStackNodes.end(); it++) {
		FileStackNode const *node = *it;

		writeFileStackNode(node, f);

		// The list is supposed to have decrementing IDs
		if (it + 1 != fileStackNodes.end() && it[1]->ID != node->ID - 1)
			fatalerror("Internal error: fstack node #%" PRIu32 " follows #%" PRIu32
				   ". Please report this to the developers!\n",
				   it[1]->ID, node->ID);
	}

	for (Symbol const *sym : objectSymbols)
		writesymbol(sym, f);

	for (Section &sect : sectionList)
		writesection(sect, f);

	putlong(assertions.size(), f);

	for (Assertion &assert : assertions)
		writeassert(assert, f);

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
