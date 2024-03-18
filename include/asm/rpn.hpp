/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_RPN_H
#define RGBDS_ASM_RPN_H

#include <stdint.h>
#include <string>
#include <vector>

#include "linkdefs.hpp"

struct Symbol;

struct Expression {
	int32_t val;        // If the expression's value is known, it's here
	std::string reason; // Why the expression is not known, if it isn't
	bool isKnown;       // Whether the expression's value is known at assembly time
	bool isSymbol;      // Whether the expression represents a symbol suitable for const diffing
	std::vector<uint8_t> rpn; // Bytes serializing the RPN expression
	uint32_t rpnPatchSize;    // Size the expression will take in the object file

	int32_t getConstVal() const;
	Symbol const *symbolOf() const;
	bool isDiffConstant(Symbol const *symName) const;

	Expression() = default;
	Expression(Expression &&) = default;
#ifdef _MSC_VER
	// MSVC and WinFlexBison won't build without this...
	Expression(Expression const &) = default;
#endif

	Expression &operator=(Expression &&) = default;
};

void rpn_Number(Expression &expr, uint32_t val);
void rpn_Symbol(Expression &expr, std::string const &symName);
void rpn_LOGNOT(Expression &expr, Expression &&src);
void rpn_BinaryOp(RPNCommand op, Expression &expr, Expression &&src1, Expression const &src2);
void rpn_HIGH(Expression &expr, Expression &&src);
void rpn_LOW(Expression &expr, Expression &&src);
void rpn_ISCONST(Expression &expr, Expression const &src);
void rpn_NEG(Expression &expr, Expression &&src);
void rpn_NOT(Expression &expr, Expression &&src);
void rpn_BankSymbol(Expression &expr, std::string const &symName);
void rpn_BankSection(Expression &expr, std::string const &sectionName);
void rpn_BankSelf(Expression &expr);
void rpn_SizeOfSection(Expression &expr, std::string const &sectionName);
void rpn_StartOfSection(Expression &expr, std::string const &sectionName);
void rpn_SizeOfSectionType(Expression &expr, SectionType type);
void rpn_StartOfSectionType(Expression &expr, SectionType type);

void rpn_CheckHRAM(Expression &expr);
void rpn_CheckRST(Expression &expr);
void rpn_CheckNBit(Expression const &expr, uint8_t n);

#endif // RGBDS_ASM_RPN_H
