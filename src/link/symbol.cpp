/* SPDX-License-Identifier: MIT */

#include <inttypes.h>
#include <map>
#include <stdlib.h>
#include <string>

#include "link/object.hpp"
#include "link/symbol.hpp"
#include "link/main.hpp"

#include "error.hpp"

std::map<std::string, struct Symbol *> symbols;

void sym_AddSymbol(struct Symbol *symbol)
{
	// Check if the symbol already exists
	if (struct Symbol *other = sym_GetSymbol(*symbol->name); other) {
		fprintf(stderr, "error: \"%s\" both in %s from ", symbol->name->c_str(),
			symbol->objFileName);
		dumpFileStack(symbol->src);
		fprintf(stderr, "(%" PRIu32 ") and in %s from ",
			symbol->lineNo, other->objFileName);
		dumpFileStack(other->src);
		fprintf(stderr, "(%" PRIu32 ")\n", other->lineNo);
		exit(1);
	}

	// If not, add it
	symbols[*symbol->name] = symbol;
}

struct Symbol *sym_GetSymbol(std::string const &name)
{
	auto search = symbols.find(name);
	return search != symbols.end() ? search->second : NULL;
}

void sym_CleanupSymbols(void)
{
	symbols.clear();
}
