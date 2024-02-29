/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_RPN_H
#define RGBDS_ASM_RPN_H

#include <stdint.h>
#include <string>

#include "linkdefs.hpp"

struct Symbol;

struct Expression {
	int32_t val; // If the expression's value is known, it's here
	std::string *reason; // Why the expression is not known, if it isn't
	bool isKnown; // Whether the expression's value is known
	bool isSymbol; // Whether the expression represents a symbol
	std::vector<uint8_t> *rpn; // Bytes serializing the RPN expression
	uint32_t rpnPatchSize; // Size the expression will take in the object file
};

// Determines if an expression is known at assembly time
static inline bool rpn_isKnown(Expression const *expr)
{
	return expr->isKnown;
}

// Determines if an expression is a symbol suitable for const diffing
static inline bool rpn_isSymbol(const Expression *expr)
{
	return expr->isSymbol;
}

void rpn_Symbol(Expression *expr, char const *symName);
void rpn_Number(Expression *expr, uint32_t i);
void rpn_LOGNOT(Expression *expr, const Expression *src);
Symbol const *rpn_SymbolOf(Expression const *expr);
bool rpn_IsDiffConstant(Expression const *src, Symbol const *symName);
void rpn_BinaryOp(enum RPNCommand op, Expression *expr, const Expression *src1, const Expression *src2);
void rpn_HIGH(Expression *expr, const Expression *src);
void rpn_LOW(Expression *expr, const Expression *src);
void rpn_ISCONST(Expression *expr, const Expression *src);
void rpn_NEG(Expression *expr, const Expression *src);
void rpn_NOT(Expression *expr, const Expression *src);
void rpn_BankSymbol(Expression *expr, char const *symName);
void rpn_BankSection(Expression *expr, char const *sectionName);
void rpn_BankSelf(Expression *expr);
void rpn_SizeOfSection(Expression *expr, char const *sectionName);
void rpn_StartOfSection(Expression *expr, char const *sectionName);
void rpn_SizeOfSectionType(Expression *expr, enum SectionType type);
void rpn_StartOfSectionType(Expression *expr, enum SectionType type);
void rpn_Free(Expression *expr);
void rpn_CheckHRAM(Expression *expr, const Expression *src);
void rpn_CheckRST(Expression *expr, const Expression *src);
void rpn_CheckNBit(Expression const *expr, uint8_t n);
int32_t rpn_GetConstVal(Expression const *expr);

#endif // RGBDS_ASM_RPN_H
