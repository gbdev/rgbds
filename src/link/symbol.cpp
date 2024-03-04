/* SPDX-License-Identifier: MIT */

#include <inttypes.h>
#include <map>
#include <stdlib.h>
#include <string>
#include <variant>

#include "link/object.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"
#include "link/main.hpp"

#include "error.hpp"
#include "helpers.hpp"

std::map<std::string, Symbol *> symbols;

void sym_AddSymbol(Symbol &symbol)
{
	// Check if the symbol already exists
	if (Symbol *other = sym_GetSymbol(symbol.name); other) {
		fprintf(stderr, "error: \"%s\" both in %s from ", symbol.name.c_str(), symbol.objFileName);
		symbol.src->dumpFileStack();
		fprintf(stderr, "(%" PRIu32 ") and in %s from ", symbol.lineNo, other->objFileName);
		other->src->dumpFileStack();
		fprintf(stderr, "(%" PRIu32 ")\n", other->lineNo);
		exit(1);
	}

	// If not, add it
	symbols[symbol.name] = &symbol;
}

Symbol *sym_GetSymbol(std::string const &name)
{
	auto search = symbols.find(name);
	return search != symbols.end() ? search->second : nullptr;
}
