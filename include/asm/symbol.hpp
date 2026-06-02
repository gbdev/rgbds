// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_SYMBOL_HPP
#define RGBDS_ASM_SYMBOL_HPP

#include <memory>
#include <stdint.h>
#include <string>
#include <time.h>
#include <utility>
#include <variant>

#include "asm/intern.hpp"
#include "asm/lexer.hpp"
#include "asm/section.hpp"

enum SymbolType : uint8_t {
	SYM_LABEL,
	SYM_EQU,
	SYM_VAR,
	SYM_MACRO,
	SYM_EQUS,
	SYM_REF // Forward reference to a label
};

struct Symbol;                    // Forward declaration for `sym_IsPC`
bool sym_IsPC(Symbol const *sym); // Forward declaration for `getSection`

struct Symbol {
	std::variant<
	    int32_t,                           // If isNumeric()
	    int32_t (*)(),                     // If isNumeric() via a callback
	    ContentSpan,                       // For SYM_MACRO
	    std::shared_ptr<std::string>,      // For SYM_EQUS
	    std::shared_ptr<std::string> (*)() // For SYM_EQUS via a callback
	    >
	    data;

	Section *section;
	std::shared_ptr<FileStackNode> src; // Where the symbol was defined
	InternedStr name;
	uint32_t fileLine; // Line where the symbol was defined

	uint32_t ID;       // ID of the symbol in the object file (`UINT32_MAX` if none)
	uint32_t defIndex; // Ordering of the symbol in the state file

	SymbolType type;

	bool isBuiltin;
	bool isExported; // Not relevant for SYM_MACRO or SYM_EQUS
	bool isQuiet;    // Only relevant for SYM_MACRO

	bool isDefined() const { return type != SYM_REF; }
	bool isNumeric() const { return type == SYM_LABEL || type == SYM_EQU || type == SYM_VAR; }
	bool isLabel() const { return type == SYM_LABEL || type == SYM_REF; }

	bool isConstant() const {
		if (type == SYM_LABEL) {
			Section const *sect = getSection();
			return sect && sect->org != UINT32_MAX;
		}
		return type == SYM_EQU || type == SYM_VAR;
	}

	Section *getSection() const { return sym_IsPC(this) ? sect_GetSymbolSection() : section; }

	int32_t getValue() const;
	int32_t getOutputValue() const;
	ContentSpan const &getMacro() const;
	std::shared_ptr<std::string> getEqus() const;
	uint32_t getConstantValue() const;
};

bool sym_IsDotScope(InternedStr symName);

void sym_ForEach(void (*callback)(Symbol &));

Symbol *sym_AddLocalLabel(InternedStr symName);
Symbol *sym_AddLabel(InternedStr symName);
Symbol *sym_AddAnonLabel();
InternedStr sym_MakeAnonLabelName(uint32_t ofs, bool neg);
void sym_Export(InternedStr symName);
Symbol *sym_AddEqu(InternedStr symName, int32_t value);
Symbol *sym_RedefEqu(InternedStr symName, int32_t value);
Symbol *sym_AddVar(InternedStr symName, int32_t value);
int32_t sym_GetRSValue();
void sym_SetRSValue(int32_t value);
// Find a symbol by exact name, bypassing expansion checks
Symbol *sym_FindExactSymbol(InternedStr symName);
// Find a symbol, possibly scoped, by name
Symbol *sym_FindScopedSymbol(InternedStr symName);
// Find a scoped symbol by name; do not return `@` or `_NARG` when they have no value
Symbol *sym_FindScopedValidSymbol(InternedStr symName);
Symbol const *sym_GetPC();
Symbol *sym_AddMacro(InternedStr symName, int32_t defLineNo, ContentSpan const &span, bool isQuiet);
Symbol *sym_Ref(InternedStr symName);
Symbol *sym_AddString(InternedStr symName, std::shared_ptr<std::string> value);
Symbol *sym_RedefString(InternedStr symName, std::shared_ptr<std::string> value);
void sym_Purge(InternedStr symName);
bool sym_IsPurgedExact(InternedStr symName);
bool sym_IsPurgedScoped(InternedStr symName);
void sym_Init(time_t now);

// Functions to save and restore the current label scopes.
std::pair<Symbol const *, Symbol const *> sym_GetCurrentLabelScopes();
void sym_SetCurrentLabelScopes(std::pair<Symbol const *, Symbol const *> newScopes);
void sym_ResetCurrentLabelScopes();

#endif // RGBDS_ASM_SYMBOL_HPP
