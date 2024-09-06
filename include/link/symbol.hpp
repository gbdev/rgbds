/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_LINK_SYMBOL_HPP
#define RGBDS_LINK_SYMBOL_HPP

// GUIDELINE: external code MUST NOT BE AWARE of the data structure used!

#include <stdint.h>
#include <string>

#include "either.hpp"
#include "linkdefs.hpp"

struct FileStackNode;
struct Section;

struct Label {
	int32_t sectionID;
	int32_t offset;
	// Extra info computed during linking
	Section *section;
};

struct Symbol {
	// Info contained in the object files
	std::string name;
	ExportLevel type;
	FileStackNode const *src;
	int32_t lineNo;
	Either<
	    int32_t, // Constants just have a numeric value
	    Label    // Label values refer to an offset within a specific section
	    >
	    data;

	Label &label() { return data.get<Label>(); }
	Label const &label() const { return data.get<Label>(); }
};

void sym_ForEach(void (*callback)(Symbol &));

void sym_AddSymbol(Symbol &symbol);

/*
 * Finds a symbol in all the defined symbols.
 * @param name The name of the symbol to look for
 * @return A pointer to the symbol, or `nullptr` if not found.
 */
Symbol *sym_GetSymbol(std::string const &name);

void sym_DumpLocalAliasedSymbols(std::string const &name);

#endif // RGBDS_LINK_SYMBOL_HPP
