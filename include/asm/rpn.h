/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_RPN_H
#define RGBDS_ASM_RPN_H

#include <stdint.h>
#include <stdbool.h>

#include "linkdefs.h"

#define MAXRPNLEN 1048576

struct Expression {
	int32_t  nVal;          // If the expression's value is known, it's here
	char     *reason;       // Why the expression is not known, if it isn't
	bool     isKnown;       // Whether the expression's value is known
	bool     isSymbol;      // Whether the expression represents a symbol
	uint8_t  *tRPN;         // Array of bytes serializing the RPN expression
	uint32_t nRPNCapacity;  // Size of the `tRPN` buffer
	uint32_t nRPNLength;    // Used size of the `tRPN` buffer
	uint32_t nRPNPatchSize; // Size the expression will take in the obj file
};

/* FIXME: Should be defined in `parser.h`, but impossible with POSIX Yacc */
extern int32_t nPCOffset;

/*
 * Determines if an expression is known at assembly time
 */
static inline bool rpn_isKnown(const struct Expression *expr)
{
	return expr->isKnown;
}

/*
 * Determines if an expression is a symbol suitable for const diffing
 */
static inline bool rpn_isSymbol(const struct Expression *expr)
{
	return expr->isSymbol;
}

void rpn_Symbol(struct Expression *expr, char const *tzSym);
void rpn_Number(struct Expression *expr, uint32_t i);
void rpn_LOGNOT(struct Expression *expr, const struct Expression *src);
struct Symbol const *rpn_SymbolOf(struct Expression const *expr);
bool rpn_IsDiffConstant(struct Expression const *src, struct Symbol const *sym);
void rpn_BinaryOp(enum RPNCommand op, struct Expression *expr,
		  const struct Expression *src1,
		  const struct Expression *src2);
void rpn_HIGH(struct Expression *expr, const struct Expression *src);
void rpn_LOW(struct Expression *expr, const struct Expression *src);
void rpn_ISCONST(struct Expression *expr, const struct Expression *src);
void rpn_UNNEG(struct Expression *expr, const struct Expression *src);
void rpn_UNNOT(struct Expression *expr, const struct Expression *src);
void rpn_BankSymbol(struct Expression *expr, char const *tzSym);
void rpn_BankSection(struct Expression *expr, char const *tzSectionName);
void rpn_BankSelf(struct Expression *expr);
void rpn_Free(struct Expression *expr);
void rpn_CheckHRAM(struct Expression *expr, const struct Expression *src);
void rpn_CheckRST(struct Expression *expr, const struct Expression *src);

#endif /* RGBDS_ASM_RPN_H */
