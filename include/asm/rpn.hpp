/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_RPN_H
#define RGBDS_ASM_RPN_H

#include <stdint.h>
#include <string>
#include <vector>

#include "linkdefs.hpp"

struct Symbol;

struct Expression {
	int32_t val;         // If the expression's value is known, it's here
	std::string reason;  // Why the expression is not known, if it isn't
	bool isKnown;        // Whether the expression's value is known at assembly time
	bool isSymbol;       // Whether the expression represents a symbol suitable for const diffing
	std::vector<uint8_t> *rpn; // Bytes serializing the RPN expression
	uint32_t rpnPatchSize;     // Size the expression will take in the object file

	int32_t getConstVal() const;
	Symbol const *symbolOf() const;
	bool isDiffConstant(Symbol const *symName) const;
};

void rpn_Number(Expression &expr, uint32_t val);
void rpn_Symbol(Expression &expr, char const *symName);
void rpn_LOGNOT(Expression &expr, const Expression &src);
void rpn_BinaryOp(
    enum RPNCommand op, Expression &expr, const Expression &src1, const Expression &src2
);
void rpn_HIGH(Expression &expr, const Expression &src);
void rpn_LOW(Expression &expr, const Expression &src);
void rpn_ISCONST(Expression &expr, const Expression &src);
void rpn_NEG(Expression &expr, const Expression &src);
void rpn_NOT(Expression &expr, const Expression &src);
void rpn_BankSymbol(Expression &expr, char const *symName);
void rpn_BankSection(Expression &expr, char const *sectionName);
void rpn_BankSelf(Expression &expr);
void rpn_SizeOfSection(Expression &expr, char const *sectionName);
void rpn_StartOfSection(Expression &expr, char const *sectionName);
void rpn_SizeOfSectionType(Expression &expr, enum SectionType type);
void rpn_StartOfSectionType(Expression &expr, enum SectionType type);

void rpn_Free(Expression &expr);

void rpn_CheckHRAM(Expression &expr, const Expression &src);
void rpn_CheckRST(Expression &expr, const Expression &src);
void rpn_CheckNBit(Expression const &expr, uint8_t n);

#endif // RGBDS_ASM_RPN_H
