// SPDX-License-Identifier: MIT

#include "link/symbol.hpp"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "helpers.hpp" // assume
#include "linkdefs.hpp"

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
		    "`%s` is defined as %s, but also as %s",
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

	size_t nbListed = 0;
	for (Symbol *local : locals) {
		if (nbListed == 3) {
			fprintf(stderr, "    ...and %zu more\n", locals.size() - nbListed);
			break;
		}
		assume(local->src);
		local->src->printBacktrace(local->lineNo);
		++nbListed;
	}
}

void Symbol::linkToSection(Section &section) {
	assume(std::holds_alternative<Label>(data));
	Label &label = std::get<Label>(data);
	// Link the symbol to the section
	label.section = &section;
	// Link the section to the symbol, keeping the section's symbol list sorted
	uint32_t a = 0, b = section.symbols.size();
	while (a != b) {
		uint32_t c = (a + b) / 2;
		assume(std::holds_alternative<Label>(section.symbols[c]->data));
		Label const &other = std::get<Label>(section.symbols[c]->data);
		if (other.offset > label.offset) {
			b = c;
		} else {
			a = c + 1;
		}
	}
	section.symbols.insert(section.symbols.begin() + a, this);
}

void Symbol::fixSectionOffset() {
	if (!std::holds_alternative<Label>(data)) {
		return;
	}

	Label &label = std::get<Label>(data);
	Section *section = label.section;
	assume(section);

	if (section->modifier != SECTION_NORMAL) {
		// Associate the symbol with the main section, not the "piece"
		label.section = sect_GetSection(section->name);
	}
	if (section->modifier == SECTION_FRAGMENT) {
		// Add the fragment's offset to the symbol's
		// (`section->offset` is computed by `sect_AddSection`)
		label.offset += section->offset;
	}
}
