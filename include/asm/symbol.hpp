/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_SYMBOL_HPP
#define RGBDS_ASM_SYMBOL_HPP

#include <memory>
#include <optional>
#include <stdint.h>
#include <string.h>
#include <string>
#include <string_view>
#include <time.h>
#include <variant>

#include "asm/lexer.hpp"
#include "asm/section.hpp"

enum SymbolType {
	SYM_LABEL,
	SYM_EQU,
	SYM_VAR,
	SYM_MACRO,
	SYM_EQUS,
	SYM_REF // Forward reference to a label
};

struct Symbol;                    // For the `sym_IsPC` forward declaration
bool sym_IsPC(Symbol const *sym); // For the inline `getSection` method

struct Symbol {
	std::string name;
	SymbolType type;
	bool isExported; // Whether the symbol is to be exported
	bool isBuiltin;  // Whether the symbol is a built-in
	Section *section;
	std::shared_ptr<FileStackNode> src; // Where the symbol was defined
	uint32_t fileLine;                  // Line where the symbol was defined

	std::variant<
	    int32_t,                     // If isNumeric()
	    int32_t (*)(),               // If isNumeric() and has a callback
	    ContentSpan,                 // For SYM_MACRO
	    std::shared_ptr<std::string> // For SYM_EQUS
	    >
	    data;

	uint32_t ID;       // ID of the symbol in the object file (-1 if none)
	uint32_t defIndex; // Ordering of the symbol in the state file

	bool isDefined() const { return type != SYM_REF; }
	bool isNumeric() const { return type == SYM_LABEL || type == SYM_EQU || type == SYM_VAR; }
	bool isLabel() const { return type == SYM_LABEL || type == SYM_REF; }

	bool isConstant() const {
		if (type == SYM_LABEL) {
			Section const *sect = getSection();
			return sect && sect->org != (uint32_t)-1;
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

void sym_ForEach(void (*callback)(Symbol &));

void sym_SetExportAll(bool set);
Symbol *sym_AddLocalLabel(std::string const &symName);
Symbol *sym_AddLabel(std::string const &symName);
Symbol *sym_AddAnonLabel();
std::string sym_MakeAnonLabelName(uint32_t ofs, bool neg);
void sym_Export(std::string const &symName);
Symbol *sym_AddEqu(std::string const &symName, int32_t value);
Symbol *sym_RedefEqu(std::string const &symName, int32_t value);
Symbol *sym_AddVar(std::string const &symName, int32_t value);
uint32_t sym_GetPCValue();
int32_t sym_GetRSValue();
void sym_SetRSValue(int32_t value);
uint32_t sym_GetConstantValue(std::string const &symName);
// Find a symbol by exact name, bypassing expansion checks
Symbol *sym_FindExactSymbol(std::string const &symName);
// Find a symbol, possibly scoped, by name
Symbol *sym_FindScopedSymbol(std::string const &symName);
// Find a scoped symbol by name; do not return `@` or `_NARG` when they have no value
Symbol *sym_FindScopedValidSymbol(std::string const &symName);
Symbol const *sym_GetPC();
Symbol *sym_AddMacro(std::string const &symName, int32_t defLineNo, ContentSpan const &span);
Symbol *sym_Ref(std::string const &symName);
Symbol *sym_AddString(std::string const &symName, std::shared_ptr<std::string> value);
Symbol *sym_RedefString(std::string const &symName, std::shared_ptr<std::string> value);
void sym_Purge(std::string const &symName);
bool sym_IsPurgedExact(std::string const &symName);
bool sym_IsPurgedScoped(std::string const &symName);
void sym_Init(time_t now);

// Functions to save and restore the current symbol scope.
std::optional<std::string> const &sym_GetCurrentSymbolScope();
void sym_SetCurrentSymbolScope(std::optional<std::string> const &newScope);

#endif // RGBDS_ASM_SYMBOL_HPP
