/* SPDX-License-Identifier: MIT */

#include "link/symbol.hpp"

#include <stdlib.h>
#include <unordered_map>

#include "helpers.hpp" // assume

#include "link/main.hpp"
#include "link/section.hpp"

std::unordered_map<std::string, Symbol *> symbols;

Label &Symbol::label() {
	assume(std::holds_alternative<Label>(data));
	return std::get<Label>(data);
}

Label const &Symbol::label() const {
	assume(std::holds_alternative<Label>(data));
	return std::get<Label>(data);
}

void sym_ForEach(void (*callback)(Symbol &)) {
	for (auto &it : symbols)
		callback(*it.second);
}

void sym_AddSymbol(Symbol &symbol) {
	Symbol *other = sym_GetSymbol(symbol.name);
	auto *symValue = std::get_if<int32_t>(&symbol.data);
	auto *otherValue = other ? std::get_if<int32_t>(&other->data) : nullptr;

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
