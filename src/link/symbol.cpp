// SPDX-License-Identifier: MIT

#include "link/symbol.hpp"

#include <inttypes.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#include "helpers.hpp" // assume

#include "link/fstack.hpp"
#include "link/section.hpp"
#include "link/warning.hpp"

static std::unordered_map<std::string, Symbol *> symbols;
static std::unordered_map<std::string, std::vector<Symbol *>> localSymbols;

void sym_ForEach(void (*callback)(Symbol &)) {
	for (auto &it : symbols) {
		callback(*it.second);
	}
}

void sym_AddSymbol(Symbol &symbol) {
	if (symbol.type != SYMTYPE_EXPORT) {
		if (symbol.type != SYMTYPE_IMPORT) {
			localSymbols[symbol.name].push_back(&symbol);
		}
		return;
	}

	Symbol *other = sym_GetSymbol(symbol.name);
	int32_t *symValue =
	    std::holds_alternative<int32_t>(symbol.data) ? &std::get<int32_t>(symbol.data) : nullptr;
	int32_t *otherValue = other && std::holds_alternative<int32_t>(other->data)
	                          ? &std::get<int32_t>(other->data)
	                          : nullptr;

	// Check if the symbol already exists with a different value
	if (other && !(symValue && otherValue && *symValue == *otherValue)) {
		std::string symDef = symValue ? std::to_string(*symValue) : "a label";
		std::string otherDef = otherValue ? std::to_string(*otherValue) : "another label";
		fatalTwoAt(
		    symbol,
		    *other,
		    "\"%s\" is defined as %s, but also as %s",
		    symbol.name.c_str(),
		    symDef.c_str(),
		    otherDef.c_str()
		);
	}

	// If not, add it (potentially replacing the previous same-value symbol)
	symbols[symbol.name] = &symbol;
}

Symbol *sym_GetSymbol(std::string const &name) {
	auto search = symbols.find(name);
	return search != symbols.end() ? search->second : nullptr;
}

void sym_TraceLocalAliasedSymbols(std::string const &name) {
	std::vector<Symbol *> const &locals = localSymbols[name];
	if (locals.empty()) {
		return;
	}

	bool plural = locals.size() != 1;
	fprintf(
	    stderr,
	    "    %zu symbol%s with that name %s defined but not exported:\n",
	    locals.size(),
	    plural ? "s" : "",
	    plural ? "are" : "is"
	);

	int count = 0;
	for (Symbol *local : locals) {
		assume(local->src);
		local->src->printBacktrace(local->lineNo);
		if (++count == 3 && locals.size() > 3) {
			fprintf(stderr, "    ...and %zu more\n", locals.size() - 3);
			break;
		}
	}
}
