/* SPDX-License-Identifier: MIT */

#include "asm/output.hpp"

#include <deque>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "error.hpp"
#include "helpers.hpp" // assume, Defer

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp"
#include "asm/rpn.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

struct Assertion {
	Patch patch;
	Section *section;
	std::string message;
};

std::string objectName;

// List of symbols to put in the object file
static std::vector<Symbol *> objectSymbols;

static std::deque<Assertion> assertions;

static std::deque<std::shared_ptr<FileStackNode>> fileStackNodes;

// Write a long to a file (little-endian)
static void putlong(uint32_t n, FILE *file) {
	uint8_t bytes[] = {
	    (uint8_t)n,
	    (uint8_t)(n >> 8),
	    (uint8_t)(n >> 16),
	    (uint8_t)(n >> 24),
	};
	fwrite(bytes, 1, sizeof(bytes), file);
}

// Write a NUL-terminated string to a file
static void putstring(std::string const &s, FILE *file) {
	fputs(s.c_str(), file);
	putc('\0', file);
}

void out_RegisterNode(std::shared_ptr<FileStackNode> node) {
	// If node is not already registered, register it (and parents), and give it a unique ID
	for (; node && node->ID == (uint32_t)-1; node = node->parent) {
		node->ID = fileStackNodes.size();
		fileStackNodes.push_front(node);
	}
}

// Return a section's ID, or -1 if the section is not in the list
static uint32_t getSectIDIfAny(Section *sect) {
	if (!sect)
		return (uint32_t)-1;

	if (auto search = sectionMap.find(sect->name); search != sectionMap.end())
		return (uint32_t)(sectionMap.size() - search->second - 1);

	fatalerror("Unknown section '%s'\n", sect->name.c_str());
}

// Write a patch to a file
static void writepatch(Patch const &patch, FILE *file) {
	assume(patch.src->ID != (uint32_t)-1);
	putlong(patch.src->ID, file);
	putlong(patch.lineNo, file);
	putlong(patch.offset, file);
	putlong(getSectIDIfAny(patch.pcSection), file);
	putlong(patch.pcOffset, file);
	putc(patch.type, file);
	putlong(patch.rpn.size(), file);
	fwrite(patch.rpn.data(), 1, patch.rpn.size(), file);
}

// Write a section to a file
static void writesection(Section const &sect, FILE *file) {
	putstring(sect.name, file);

	putlong(sect.size, file);

	bool isUnion = sect.modifier == SECTION_UNION;
	bool isFragment = sect.modifier == SECTION_FRAGMENT;

	putc(sect.type | isUnion << 7 | isFragment << 6, file);

	putlong(sect.org, file);
	putlong(sect.bank, file);
	putc(sect.align, file);
	putlong(sect.alignOfs, file);

	if (sect_HasData(sect.type)) {
		fwrite(sect.data.data(), 1, sect.size, file);
		putlong(sect.patches.size(), file);

		for (Patch const &patch : sect.patches)
			writepatch(patch, file);
	}
}

// Write a symbol to a file
static void writesymbol(Symbol const &sym, FILE *file) {
	putstring(sym.name, file);
	if (!sym.isDefined()) {
		putc(SYMTYPE_IMPORT, file);
	} else {
		assume(sym.src->ID != (uint32_t)-1);

		putc(sym.isExported ? SYMTYPE_EXPORT : SYMTYPE_LOCAL, file);
		putlong(sym.src->ID, file);
		putlong(sym.fileLine, file);
		putlong(getSectIDIfAny(sym.getSection()), file);
		putlong(sym.getOutputValue(), file);
	}
}

static void registerUnregisteredSymbol(Symbol &sym) {
	// Check for `sym.src`, to skip any built-in symbol from rgbasm
	if (sym.src && sym.ID == (uint32_t)-1 && !sym_IsPC(&sym)) {
		sym.ID = objectSymbols.size(); // Set the symbol's ID within the object file
		objectSymbols.push_back(&sym);
		out_RegisterNode(sym.src);
	}
}

static void writerpn(std::vector<uint8_t> &rpnexpr, std::vector<uint8_t> const &rpn) {
	std::string symName;
	size_t rpnptr = 0;

	for (size_t offset = 0; offset < rpn.size();) {
		uint8_t rpndata = rpn[offset++];

		switch (rpndata) {
			Symbol *sym;
			uint32_t value;
			uint8_t b;

		case RPN_CONST:
			rpnexpr[rpnptr++] = RPN_CONST;
			rpnexpr[rpnptr++] = rpn[offset++];
			rpnexpr[rpnptr++] = rpn[offset++];
			rpnexpr[rpnptr++] = rpn[offset++];
			rpnexpr[rpnptr++] = rpn[offset++];
			break;

		case RPN_SYM:
			symName.clear();
			for (;;) {
				uint8_t c = rpn[offset++];
				if (c == 0)
					break;
				symName += c;
			}

			// The symbol name is always written expanded
			sym = sym_FindExactSymbol(symName);
			if (sym->isConstant()) {
				rpnexpr[rpnptr++] = RPN_CONST;
				value = sym_GetConstantValue(symName);
			} else {
				rpnexpr[rpnptr++] = RPN_SYM;
				registerUnregisteredSymbol(*sym); // Ensure that `sym->ID` is set
				value = sym->ID;
			}

			rpnexpr[rpnptr++] = value & 0xFF;
			rpnexpr[rpnptr++] = value >> 8;
			rpnexpr[rpnptr++] = value >> 16;
			rpnexpr[rpnptr++] = value >> 24;
			break;

		case RPN_BANK_SYM:
			symName.clear();
			for (;;) {
				uint8_t c = rpn[offset++];
				if (c == 0)
					break;
				symName += c;
			}

			// The symbol name is always written expanded
			sym = sym_FindExactSymbol(symName);
			registerUnregisteredSymbol(*sym); // Ensure that `sym->ID` is set
			value = sym->ID;

			rpnexpr[rpnptr++] = RPN_BANK_SYM;
			rpnexpr[rpnptr++] = value & 0xFF;
			rpnexpr[rpnptr++] = value >> 8;
			rpnexpr[rpnptr++] = value >> 16;
			rpnexpr[rpnptr++] = value >> 24;
			break;

		case RPN_BANK_SECT:
			rpnexpr[rpnptr++] = RPN_BANK_SECT;
			do {
				b = rpn[offset++];
				rpnexpr[rpnptr++] = b;
			} while (b != 0);
			break;

		case RPN_SIZEOF_SECT:
			rpnexpr[rpnptr++] = RPN_SIZEOF_SECT;
			do {
				b = rpn[offset++];
				rpnexpr[rpnptr++] = b;
			} while (b != 0);
			break;

		case RPN_STARTOF_SECT:
			rpnexpr[rpnptr++] = RPN_STARTOF_SECT;
			do {
				b = rpn[offset++];
				rpnexpr[rpnptr++] = b;
			} while (b != 0);
			break;

		default:
			rpnexpr[rpnptr++] = rpndata;
			break;
		}
	}
}

static void initpatch(Patch &patch, uint32_t type, Expression const &expr, uint32_t ofs) {
	patch.type = type;
	patch.src = fstk_GetFileStack();
	// All patches are assumed to eventually be written, so the file stack node is registered
	out_RegisterNode(patch.src);
	patch.lineNo = lexer_GetLineNo();
	patch.offset = ofs;
	patch.pcSection = sect_GetSymbolSection();
	patch.pcOffset = sect_GetSymbolOffset();

	if (expr.isKnown()) {
		// If the RPN expr's value is known, output a constant directly
		uint32_t val = expr.value();
		patch.rpn.resize(5);
		patch.rpn[0] = RPN_CONST;
		patch.rpn[1] = val & 0xFF;
		patch.rpn[2] = val >> 8;
		patch.rpn[3] = val >> 16;
		patch.rpn[4] = val >> 24;
	} else {
		patch.rpn.resize(expr.rpnPatchSize);
		writerpn(patch.rpn, expr.rpn);
	}
}

// Create a new patch (includes the rpn expr)
void out_CreatePatch(uint32_t type, Expression const &expr, uint32_t ofs, uint32_t pcShift) {
	// Add the patch to the list
	Patch &patch = currentSection->patches.emplace_front();

	initpatch(patch, type, expr, ofs);

	// If the patch had a quantity of bytes output before it,
	// PC is not at the patch's location, but at the location
	// before those bytes.
	patch.pcOffset -= pcShift;
}

// Creates an assert that will be written to the object file
void out_CreateAssert(
    AssertionType type, Expression const &expr, std::string const &message, uint32_t ofs
) {
	Assertion &assertion = assertions.emplace_front();

	initpatch(assertion.patch, type, expr, ofs);
	assertion.message = message;
}

static void writeassert(Assertion &assert, FILE *file) {
	writepatch(assert.patch, file);
	putstring(assert.message, file);
}

static void writeFileStackNode(FileStackNode const &node, FILE *file) {
	putlong(node.parent ? node.parent->ID : (uint32_t)-1, file);
	putlong(node.lineNo, file);
	putc(node.type, file);
	if (node.type != NODE_REPT) {
		putstring(node.name(), file);
	} else {
		std::vector<uint32_t> const &nodeIters = node.iters();

		putlong(nodeIters.size(), file);
		// Iters are stored by decreasing depth, so reverse the order for output
		for (uint32_t i = nodeIters.size(); i--;)
			putlong(nodeIters[i], file);
	}
}

// Write an object file
void out_WriteObject() {
	FILE *file;
	if (objectName != "-") {
		file = fopen(objectName.c_str(), "wb");
	} else {
		objectName = "<stdout>";
		file = fdopen(STDOUT_FILENO, "wb");
	}
	if (!file)
		err("Failed to open object file '%s'", objectName.c_str());
	Defer closeFile{[&] { fclose(file); }};

	// Also write symbols that weren't written above
	sym_ForEach(registerUnregisteredSymbol);

	fprintf(file, RGBDS_OBJECT_VERSION_STRING);
	putlong(RGBDS_OBJECT_REV, file);

	putlong(objectSymbols.size(), file);
	putlong(sectionList.size(), file);

	putlong(fileStackNodes.size(), file);
	for (auto it = fileStackNodes.begin(); it != fileStackNodes.end(); it++) {
		FileStackNode const &node = **it;

		writeFileStackNode(node, file);

		// The list is supposed to have decrementing IDs
		if (it + 1 != fileStackNodes.end() && it[1]->ID != node.ID - 1)
			fatalerror(
			    "Internal error: fstack node #%" PRIu32 " follows #%" PRIu32
			    ". Please report this to the developers!\n",
			    it[1]->ID,
			    node.ID
			);
	}

	for (Symbol const *sym : objectSymbols)
		writesymbol(*sym, file);

	for (auto it = sectionList.rbegin(); it != sectionList.rend(); it++)
		writesection(*it, file);

	putlong(assertions.size(), file);

	for (Assertion &assert : assertions)
		writeassert(assert, file);
}

// Set the object filename
void out_SetFileName(std::string const &name) {
	if (!objectName.empty())
		warnx("Overriding output filename %s", objectName.c_str());
	objectName = name;
	if (verbose)
		printf("Output filename %s\n", objectName.c_str());
}
