/* SPDX-License-Identifier: MIT */

// Declarations manipulating symbols
#ifndef RGBDS_LINK_SYMBOL_H
#define RGBDS_LINK_SYMBOL_H

// GUIDELINE: external code MUST NOT BE AWARE of the data structure used!

#include <stdint.h>
#include <string>

#include "linkdefs.hpp"

struct FileStackNode;

struct Symbol {
	// Info contained in the object files
	std::string *name;
	enum ExportLevel type;
	char const *objFileName;
	struct FileStackNode const *src;
	int32_t lineNo;
	int32_t sectionID;
	union {
		// Both types must be identical
		int32_t offset;
		int32_t value;
	};
	// Extra info computed during linking
	struct Section *section;
};

void sym_AddSymbol(struct Symbol *symbol);

/*
 * Finds a symbol in all the defined symbols.
 * @param name The name of the symbol to look for
 * @return A pointer to the symbol, or NULL if not found.
 */
struct Symbol *sym_GetSymbol(std::string const &name);

/*
 * `free`s all symbol memory that was allocated.
 */
void sym_CleanupSymbols(void);

#endif // RGBDS_LINK_SYMBOL_H
