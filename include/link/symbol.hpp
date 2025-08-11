// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_SYMBOL_HPP
#define RGBDS_LINK_SYMBOL_HPP

// GUIDELINE: external code MUST NOT BE AWARE of the data structure used!

#include <stdint.h>
#include <string>
#include <variant>

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
	std::variant<
	    int32_t, // Constants just have a numeric value
	    Label    // Label values refer to an offset within a specific section
	    >
	    data;

	Label &label() { return std::get<Label>(data); }
	Label const &label() const { return std::get<Label>(data); }
};

void sym_ForEach(void (*callback)(Symbol &));

void sym_AddSymbol(Symbol &symbol);

// Finds a symbol in all the defined symbols.
Symbol *sym_GetSymbol(std::string const &name);

void sym_TraceLocalAliasedSymbols(std::string const &name);

#endif // RGBDS_LINK_SYMBOL_HPP
