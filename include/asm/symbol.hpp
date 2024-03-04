/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_SYMBOL_H
#define RGBDS_SYMBOL_H

#include <stdint.h>
#include <string.h>
#include <string>
#include <string_view>
#include <time.h>
#include <variant>

#include "asm/section.hpp"

#define MAXSYMLEN 255

enum SymbolType {
	SYM_LABEL,
	SYM_EQU,
	SYM_VAR,
	SYM_MACRO,
	SYM_EQUS,
	SYM_REF // Forward reference to a label
};

struct Symbol; // For the `sym_IsPC` forward declaration
bool sym_IsPC(Symbol const *sym); // For the inline `getSection` method

struct Symbol {
	char name[MAXSYMLEN + 1];
	enum SymbolType type;
	bool isExported; // Whether the symbol is to be exported
	bool isBuiltin; // Whether the symbol is a built-in
	Section *section;
	FileStackNode *src; // Where the symbol was defined
	uint32_t fileLine; // Line where the symbol was defined

	std::variant<int32_t, // If isNumeric()
	             int32_t (*)(), // If isNumeric() and has a callback
	             std::string_view *, // For SYM_MACRO
	             std::string * // For SYM_EQUS
	             >
	    data;

	uint32_t ID; // ID of the symbol in the object file (-1 if none)

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
	std::string_view *getMacro() const;
	std::string *getEqus() const;
	uint32_t getConstantValue() const;
};

void sym_ForEach(void (*func)(Symbol &));

void sym_SetExportAll(bool set);
Symbol *sym_AddLocalLabel(char const *symName);
Symbol *sym_AddLabel(char const *symName);
Symbol *sym_AddAnonLabel();
void sym_WriteAnonLabelName(char buf[MAXSYMLEN + 1], uint32_t ofs, bool neg);
void sym_Export(char const *symName);
Symbol *sym_AddEqu(char const *symName, int32_t value);
Symbol *sym_RedefEqu(char const *symName, int32_t value);
Symbol *sym_AddVar(char const *symName, int32_t value);
uint32_t sym_GetPCValue();
uint32_t sym_GetConstantValue(char const *symName);
// Find a symbol by exact name, bypassing expansion checks
Symbol *sym_FindExactSymbol(char const *symName);
// Find a symbol, possibly scoped, by name
Symbol *sym_FindScopedSymbol(char const *symName);
// Find a scoped symbol by name; do not return `@` or `_NARG` when they have no value
Symbol *sym_FindScopedValidSymbol(char const *symName);
Symbol const *sym_GetPC();
Symbol *sym_AddMacro(char const *symName, int32_t defLineNo, char const *body, size_t size);
Symbol *sym_Ref(char const *symName);
Symbol *sym_AddString(char const *symName, char const *value);
Symbol *sym_RedefString(char const *symName, char const *value);
void sym_Purge(std::string const &symName);
void sym_Init(time_t now);

// Functions to save and restore the current symbol scope.
char const *sym_GetCurrentSymbolScope();
void sym_SetCurrentSymbolScope(char const *newScope);

#endif // RGBDS_SYMBOL_H
