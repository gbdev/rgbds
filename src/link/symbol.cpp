/* SPDX-License-Identifier: MIT */

#include "link/symbol.hpp"

#include <inttypes.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#include "helpers.hpp" // assume

#include "link/main.hpp"
#include "link/section.hpp"

std::unordered_map<std::string, Symbol *> symbols;
std::unordered_map<std::string, std::vector<Symbol *>> localSymbols;

void sym_ForEach(void (*callback)(Symbol &)) {
	for (auto &it : symbols)
		callback(*it.second);
}

void sym_AddSymbol(Symbol &symbol) {
	if (symbol.type != SYMTYPE_EXPORT) {
		if (symbol.type != SYMTYPE_IMPORT)
			localSymbols[symbol.name].push_back(&symbol);
		return;
	}

	Symbol *other = sym_GetSymbol(symbol.name);
	int32_t *symValue = symbol.data.holds<int32_t>() ? &symbol.data.get<int32_t>() : nullptr;
	int32_t *otherValue =
	    other && other->data.holds<int32_t>() ? &other->data.get<int32_t>() : nullptr;

	// Check if the symbol already exists with a different value
	if (other && !(symValue && otherValue && *symValue == *otherValue)) {
		fprintf(stderr, "error: \"%s\" is defined as ", symbol.name.c_str());
		if (symValue)
			fprintf(stderr, "%" PRId32, *symValue);
		else
			fputs("a label", stderr);
		fputs(" at ", stderr);
		symbol.src->dump(symbol.lineNo);
		fputs(", but as ", stderr);
		if (otherValue)
			fprintf(stderr, "%" PRId32, *otherValue);
		else
			fputs("another label", stderr);
		fputs(" at ", stderr);
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

void sym_DumpLocalAliasedSymbols(std::string const &name) {
	std::vector<Symbol *> const &locals = localSymbols[name];
	int count = 0;
	for (Symbol *local : locals) {
		if (count++ == 3) {
			size_t remaining = locals.size() - 3;
			bool plural = remaining != 1;
			fprintf(
			    stderr,
			    "    ...and %zu more symbol%s with that name %s defined but not exported\n",
			    remaining,
			    plural ? "s" : "",
			    plural ? "are" : "is"
			);
			break;
		}
		fprintf(
		    stderr,
		    "    A %s with that name is defined but not exported at ",
		    local->data.holds<Label>() ? "label" : "constant"
		);
		assume(local->src);
		local->src->dump(local->lineNo);
		putc('\n', stderr);
	}
}
