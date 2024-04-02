/* SPDX-License-Identifier: MIT */

#include "link/symbol.hpp"

#include <stdlib.h>
#include <unordered_map>

#include "helpers.hpp" // assume

#include "link/main.hpp"
#include "link/section.hpp"

std::unordered_map<std::string, Symbol *> symbols;

void sym_ForEach(void (*callback)(Symbol &)) {
	for (auto &it : symbols)
		callback(*it.second);
}

void sym_AddSymbol(Symbol &symbol) {
	Symbol *other = sym_GetSymbol(symbol.name);
	int32_t *symValue = symbol.data.holds<int32_t>() ? &symbol.data.get<int32_t>() : nullptr;
	int32_t *otherValue =
	    other && other->data.holds<int32_t>() ? &other->data.get<int32_t>() : nullptr;

	// Check if the symbol already exists with a different value
	if (other && !(symValue && otherValue && *symValue == *otherValue)) {
		fprintf(stderr, "error: \"%s\" both in %s from ", symbol.name.c_str(), symbol.objFileName);
		symbol.src->dump(symbol.lineNo);
		fprintf(stderr, " and in %s from ", other->objFileName);
		other->src->dump(other->lineNo);
		putc('\n', stderr);
		exit(1);
	}

	// If not, add it (potentially replacing the previous same-value symbol)
	symbols[symbol.name] = &symbol;
}

Symbol *sym_GetSymbol(std::string const &name) {
	auto search = symbols.find(name);
	return search != symbols.end() ? search->second : nullptr;
}
