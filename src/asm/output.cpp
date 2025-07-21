// SPDX-License-Identifier: MIT

#include "asm/output.hpp"

#include <algorithm>
#include <deque>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "diagnostics.hpp"
#include "helpers.hpp" // assume, Defer
#include "platform.hpp"

#include "asm/charmap.hpp"
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

// List of symbols to put in the object file
static std::vector<Symbol *> objectSymbols;

static std::deque<Assertion> assertions;

static std::deque<std::shared_ptr<FileStackNode>> fileStackNodes;

static void putLong(uint32_t n, FILE *file) {
	uint8_t bytes[] = {
	    static_cast<uint8_t>(n),
	    static_cast<uint8_t>(n >> 8),
	    static_cast<uint8_t>(n >> 16),
	    static_cast<uint8_t>(n >> 24),
	};
	fwrite(bytes, 1, sizeof(bytes), file);
}

static void putString(std::string const &s, FILE *file) {
	fputs(s.c_str(), file);
	putc('\0', file);
}

void out_RegisterNode(std::shared_ptr<FileStackNode> node) {
	// If node is not already registered, register it (and parents), and give it a unique ID
	for (; node && node->ID == UINT32_MAX; node = node->parent) {
		node->ID = fileStackNodes.size();
		fileStackNodes.push_front(node);
	}
}

// Return a section's ID, or UINT32_MAX if the section does not exist
static uint32_t getSectIDIfAny(Section *sect) {
	if (!sect) {
		return UINT32_MAX;
	}

	// Search in `sectionList` instead of `sectionMap`, since section fragments share the
	// same name but have different IDs
	if (auto search =
	        std::find_if(RANGE(sectionList), [&sect](Section const &s) { return &s == sect; });
	    search != sectionList.end()) {
		return static_cast<uint32_t>(std::distance(sectionList.begin(), search));
	}

	// Every section that exists should be in `sectionMap`
	fatal("Unknown section '%s'", sect->name.c_str()); // LCOV_EXCL_LINE
}

static void writePatch(Patch const &patch, FILE *file) {
	assume(patch.src->ID != UINT32_MAX);

	putLong(patch.src->ID, file);
	putLong(patch.lineNo, file);
	putLong(patch.offset, file);
	putLong(getSectIDIfAny(patch.pcSection), file);
	putLong(patch.pcOffset, file);
	putc(patch.type, file);
	putLong(patch.rpn.size(), file);
	fwrite(patch.rpn.data(), 1, patch.rpn.size(), file);
}

static void writeSection(Section const &sect, FILE *file) {
	assume(sect.src->ID != UINT32_MAX);

	putString(sect.name, file);

	putLong(sect.src->ID, file);
	putLong(sect.fileLine, file);

	putLong(sect.size, file);

	bool isUnion = sect.modifier == SECTION_UNION;
	bool isFragment = sect.modifier == SECTION_FRAGMENT;

	putc(sect.type | isUnion << 7 | isFragment << 6, file);

	putLong(sect.org, file);
	putLong(sect.bank, file);
	putc(sect.align, file);
	putLong(sect.alignOfs, file);

	if (sect_HasData(sect.type)) {
		fwrite(sect.data.data(), 1, sect.size, file);
		putLong(sect.patches.size(), file);

		for (Patch const &patch : sect.patches) {
			writePatch(patch, file);
		}
	}
}

static void writeSymbol(Symbol const &sym, FILE *file) {
	putString(sym.name, file);
	if (!sym.isDefined()) {
		putc(SYMTYPE_IMPORT, file);
	} else {
		assume(sym.src->ID != UINT32_MAX);

		putc(sym.isExported ? SYMTYPE_EXPORT : SYMTYPE_LOCAL, file);
		putLong(sym.src->ID, file);
		putLong(sym.fileLine, file);
		putLong(getSectIDIfAny(sym.getSection()), file);
		putLong(sym.getOutputValue(), file);
	}
}

static void registerUnregisteredSymbol(Symbol &sym) {
	// Check for `sym.src`, to skip any built-in symbol from rgbasm
	if (sym.src && sym.ID == UINT32_MAX && !sym_IsPC(&sym)) {
		sym.ID = objectSymbols.size(); // Set the symbol's ID within the object file
		objectSymbols.push_back(&sym);
		out_RegisterNode(sym.src);
	}
}

static void writeRpn(std::vector<uint8_t> &rpnexpr, std::vector<uint8_t> const &rpn) {
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
				if (c == 0) {
					break;
				}
				symName += c;
			}

			// The symbol name is always written expanded
			sym = sym_FindExactSymbol(symName);
			if (sym->isConstant()) {
				rpnexpr[rpnptr++] = RPN_CONST;
				value = sym->getConstantValue();
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
				if (c == 0) {
					break;
				}
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

static void initPatch(Patch &patch, uint32_t type, Expression const &expr, uint32_t ofs) {
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
		writeRpn(patch.rpn, expr.rpn);
	}
}

void out_CreatePatch(uint32_t type, Expression const &expr, uint32_t ofs, uint32_t pcShift) {
	// Add the patch to the list
	Patch &patch = currentSection->patches.emplace_front();

	initPatch(patch, type, expr, ofs);

	// If the patch had a quantity of bytes output before it,
	// PC is not at the patch's location, but at the location
	// before those bytes.
	patch.pcOffset -= pcShift;
}

void out_CreateAssert(
    AssertionType type, Expression const &expr, std::string const &message, uint32_t ofs
) {
	Assertion &assertion = assertions.emplace_front();

	initPatch(assertion.patch, type, expr, ofs);
	assertion.message = message;
}

static void writeAssert(Assertion const &assert, FILE *file) {
	writePatch(assert.patch, file);
	putString(assert.message, file);
}

static void writeFileStackNode(FileStackNode const &node, FILE *file) {
	putLong(node.parent ? node.parent->ID : UINT32_MAX, file);
	putLong(node.lineNo, file);
	putc(node.type, file);
	if (node.type != NODE_REPT) {
		putString(node.name(), file);
	} else {
		std::vector<uint32_t> const &nodeIters = node.iters();

		putLong(nodeIters.size(), file);
		// Iters are stored by decreasing depth, so reverse the order for output
		for (uint32_t i = nodeIters.size(); i--;) {
			putLong(nodeIters[i], file);
		}
	}
}

void out_WriteObject() {
	if (options.objectFileName.empty()) {
		return;
	}

	FILE *file;
	if (options.objectFileName != "-") {
		file = fopen(options.objectFileName.c_str(), "wb");
	} else {
		options.objectFileName = "<stdout>";
		(void)setmode(STDOUT_FILENO, O_BINARY);
		file = stdout;
	}
	if (!file) {
		// LCOV_EXCL_START
		fatal(
		    "Failed to open object file '%s': %s", options.objectFileName.c_str(), strerror(errno)
		);
		// LCOV_EXCL_STOP
	}
	Defer closeFile{[&] { fclose(file); }};

	// Also write symbols that weren't written above
	sym_ForEach(registerUnregisteredSymbol);

	fputs(RGBDS_OBJECT_VERSION_STRING, file);
	putLong(RGBDS_OBJECT_REV, file);

	putLong(objectSymbols.size(), file);
	putLong(sectionList.size(), file);

	putLong(fileStackNodes.size(), file);
	for (auto it = fileStackNodes.begin(); it != fileStackNodes.end(); it++) {
		FileStackNode const &node = **it;

		writeFileStackNode(node, file);

		// The list is supposed to have decrementing IDs
		assume(it + 1 == fileStackNodes.end() || it[1]->ID == node.ID - 1);
	}

	for (Symbol const *sym : objectSymbols) {
		writeSymbol(*sym, file);
	}

	for (Section const &sect : sectionList) {
		writeSection(sect, file);
	}

	putLong(assertions.size(), file);

	for (Assertion const &assert : assertions) {
		writeAssert(assert, file);
	}
}

void out_SetFileName(std::string const &name) {
	if (!options.objectFileName.empty()) {
		warnx("Overriding output filename %s", options.objectFileName.c_str());
	}
	options.objectFileName = name;
	verbosePrint("Output filename %s\n", options.objectFileName.c_str()); // LCOV_EXCL_LINE
}

static void dumpString(std::string const &escape, FILE *file) {
	for (char c : escape) {
		// Escape characters that need escaping
		switch (c) {
		case '\n':
			fputs("\\n", file);
			break;
		case '\r':
			fputs("\\r", file);
			break;
		case '\t':
			fputs("\\t", file);
			break;
		case '\0':
			fputs("\\0", file);
			break;
		case '\\':
		case '"':
		case '{':
			putc('\\', file);
			[[fallthrough]];
		default:
			putc(c, file);
			break;
		}
	}
}

// Symbols are ordered by file, then by definition order
static bool compareSymbols(Symbol const *sym1, Symbol const *sym2) {
	return sym1->defIndex < sym2->defIndex;
}

static bool dumpEquConstants(FILE *file) {
	static std::vector<Symbol *> equConstants; // `static` so `sym_ForEach` callback can see it
	equConstants.clear();

	sym_ForEach([](Symbol &sym) {
		if (!sym.isBuiltin && sym.type == SYM_EQU) {
			equConstants.push_back(&sym);
		}
	});
	std::sort(RANGE(equConstants), compareSymbols);

	for (Symbol const *sym : equConstants) {
		uint32_t value = static_cast<uint32_t>(sym->getOutputValue());
		fprintf(file, "def %s equ $%" PRIx32 "\n", sym->name.c_str(), value);
	}

	return !equConstants.empty();
}

static bool dumpVariables(FILE *file) {
	static std::vector<Symbol *> variables; // `static` so `sym_ForEach` callback can see it
	variables.clear();

	sym_ForEach([](Symbol &sym) {
		if (!sym.isBuiltin && sym.type == SYM_VAR) {
			variables.push_back(&sym);
		}
	});
	std::sort(RANGE(variables), compareSymbols);

	for (Symbol const *sym : variables) {
		uint32_t value = static_cast<uint32_t>(sym->getOutputValue());
		fprintf(file, "def %s = $%" PRIx32 "\n", sym->name.c_str(), value);
	}

	return !variables.empty();
}

static bool dumpEqusConstants(FILE *file) {
	static std::vector<Symbol *> equsConstants; // `static` so `sym_ForEach` callback can see it
	equsConstants.clear();

	sym_ForEach([](Symbol &sym) {
		if (!sym.isBuiltin && sym.type == SYM_EQUS) {
			equsConstants.push_back(&sym);
		}
	});
	std::sort(RANGE(equsConstants), compareSymbols);

	for (Symbol const *sym : equsConstants) {
		fprintf(file, "def %s equs \"", sym->name.c_str());
		dumpString(*sym->getEqus(), file);
		fputs("\"\n", file);
	}

	return !equsConstants.empty();
}

static bool dumpCharmaps(FILE *file) {
	static FILE *charmapFile; // `static` so `charmap_ForEach` callbacks can see it
	charmapFile = file;

	// Characters are ordered by charmap, then by definition order
	return charmap_ForEach(
	    [](std::string const &name) { fprintf(charmapFile, "newcharmap %s\n", name.c_str()); },
	    [](std::string const &mapping, std::vector<int32_t> value) {
		    fputs("charmap \"", charmapFile);
		    dumpString(mapping, charmapFile);
		    putc('"', charmapFile);
		    for (int32_t v : value) {
			    fprintf(charmapFile, ", $%" PRIx32, v);
		    }
		    putc('\n', charmapFile);
	    }
	);
}

static bool dumpMacros(FILE *file) {
	static std::vector<Symbol *> macros; // `static` so `sym_ForEach` callback can see it
	macros.clear();

	sym_ForEach([](Symbol &sym) {
		if (!sym.isBuiltin && sym.type == SYM_MACRO) {
			macros.push_back(&sym);
		}
	});
	std::sort(RANGE(macros), compareSymbols);

	for (Symbol const *sym : macros) {
		ContentSpan const &body = sym->getMacro();
		fprintf(file, "macro %s\n", sym->name.c_str());
		fwrite(body.ptr.get(), 1, body.size, file);
		fputs("endm\n", file);
	}

	return !macros.empty();
}

void out_WriteState(std::string name, std::vector<StateFeature> const &features) {
	// State files may include macro bodies, which may contain arbitrary characters,
	// so output as binary to preserve them.
	FILE *file;
	if (name != "-") {
		file = fopen(name.c_str(), "wb");
	} else {
		name = "<stdout>";
		(void)setmode(STDOUT_FILENO, O_BINARY);
		file = stdout;
	}
	if (!file) {
		// LCOV_EXCL_START
		fatal("Failed to open state file '%s': %s", name.c_str(), strerror(errno));
		// LCOV_EXCL_STOP
	}
	Defer closeFile{[&] { fclose(file); }};

	static char const *dumpHeadings[NB_STATE_FEATURES] = {
	    "Numeric constants",
	    "Variables",
	    "String constants",
	    "Character maps",
	    "Macros",
	};
	static bool (* const dumpFuncs[NB_STATE_FEATURES])(FILE *) = {
	    dumpEquConstants,
	    dumpVariables,
	    dumpEqusConstants,
	    dumpCharmaps,
	    dumpMacros,
	};

	fputs("; File generated by rgbasm\n", file);
	for (StateFeature feature : features) {
		fprintf(file, "\n; %s\n", dumpHeadings[feature]);
		if (!dumpFuncs[feature](file)) {
			fputs("; No values\n", file);
		}
	}
}
