/* SPDX-License-Identifier: MIT */

#include "link/symbol.hpp"

#include <inttypes.h>
#include <map>
#include <stdlib.h>

#include "error.hpp"
#include "helpers.hpp"

#include "link/main.hpp"
#include "link/object.hpp"
#include "link/section.hpp"

std::map<std::string, Symbol *> symbols;

Label &Symbol::label() {
	assert(std::holds_alternative<Label>(data));
	return std::get<Label>(data);
}

Label const &Symbol::label() const {
	assert(std::holds_alternative<Label>(data));
	return std::get<Label>(data);
}

void sym_AddSymbol(Symbol &symbol) {
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

Symbol *sym_GetSymbol(std::string const &name) {
	auto search = symbols.find(name);
	return search != symbols.end() ? search->second : nullptr;
}
