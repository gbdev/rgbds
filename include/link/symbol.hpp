/* SPDX-License-Identifier: MIT */

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
	char const *objFileName;
	FileStackNode const *src;
	int32_t lineNo;
	std::variant<
	    int32_t, // Constants just have a numeric value
	    Label    // Label values refer to an offset within a specific section
	    >
	    data;

	Label &label();
	Label const &label() const;
};

void sym_ForEach(void (*callback)(Symbol &));

void sym_AddSymbol(Symbol &symbol);

/*
 * Finds a symbol in all the defined symbols.
 * @param name The name of the symbol to look for
 * @return A pointer to the symbol, or `nullptr` if not found.
 */
Symbol *sym_GetSymbol(std::string const &name);

#endif // RGBDS_LINK_SYMBOL_HPP
