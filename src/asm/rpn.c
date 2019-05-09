/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * Controls RPN expressions for objectfiles
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/symbol.h"

#include "linkdefs.h"

void mergetwoexpressions(struct Expression *expr, const struct Expression *src1,
			 const struct Expression *src2)
{
	assert(src1->tRPN != NULL && src2->tRPN != NULL);

	if (src1->nRPNLength > UINT32_MAX - src2->nRPNLength)
		fatalerror("RPN expression is too large");

	uint32_t len = src1->nRPNLength + src2->nRPNLength;

	expr->tRPN = src1->tRPN;

	if (src1->nRPNCapacity >= len) {
		expr->nRPNCapacity = src1->nRPNCapacity;
	} else {
		uint32_t cap1 = src1->nRPNCapacity;
		uint32_t cap2 = src2->nRPNCapacity;
		uint32_t cap = (cap1 > cap2) ? cap1 : cap2;

		if (len > cap)
			cap = (cap <= UINT32_MAX / 2) ? cap * 2 : len;

		expr->nRPNCapacity = cap;
		expr->tRPN = realloc(expr->tRPN, expr->nRPNCapacity);
		if (expr->tRPN == NULL)
			fatalerror("No memory for RPN expression");
	}

	memcpy(expr->tRPN + src1->nRPNLength, src2->tRPN, src2->nRPNLength);
	free(src2->tRPN);

	expr->nRPNLength = len;
	expr->isReloc = src1->isReloc || src2->isReloc;
}

#define joinexpr() mergetwoexpressions(expr, src1, src2)

/*
 * Add a byte to the RPN expression
 */
void pushbyte(struct Expression *expr, int b)
{
	if (expr->nRPNLength == expr->nRPNCapacity) {
		if (expr->nRPNCapacity == 0)
			expr->nRPNCapacity = 256;
		else if (expr->nRPNCapacity > UINT32_MAX / 2)
			fatalerror("RPN expression is too large");
		else
			expr->nRPNCapacity *= 2;
		expr->tRPN = realloc(expr->tRPN, expr->nRPNCapacity);

		if (expr->tRPN == NULL)
			fatalerror("No memory for RPN expression");
	}

	expr->tRPN[expr->nRPNLength++] = b & 0xFF;
}

/*
 * Init the RPN expression
 */
void rpn_Init(struct Expression *expr)
{
	expr->tRPN = NULL;
	expr->nRPNCapacity = 0;
	expr->nRPNLength = 0;
	expr->nRPNOut = 0;
	expr->isReloc = 0;
}

/*
 * Free the RPN expression
 */
void rpn_Free(struct Expression *expr)
{
	free(expr->tRPN);
	rpn_Init(expr);
}

/*
 * Returns the next rpn byte in expression
 */
uint16_t rpn_PopByte(struct Expression *expr)
{
	if (expr->nRPNOut == expr->nRPNLength)
		return 0xDEAD;

	return expr->tRPN[expr->nRPNOut++];
}

/*
 * Determine if the current expression is relocatable
 */
uint32_t rpn_isReloc(const struct Expression *expr)
{
	return expr->isReloc;
}

/*
 * Add symbols, constants and operators to expression
 */
void rpn_Number(struct Expression *expr, uint32_t i)
{
	rpn_Init(expr);
	pushbyte(expr, RPN_CONST);
	pushbyte(expr, i);
	pushbyte(expr, i >> 8);
	pushbyte(expr, i >> 16);
	pushbyte(expr, i >> 24);
	expr->nVal = i;
}

void rpn_Symbol(struct Expression *expr, char *tzSym)
{
	if (!sym_isConstant(tzSym)) {
		rpn_Init(expr);
		sym_Ref(tzSym);
		expr->isReloc = 1;
		pushbyte(expr, RPN_SYM);
		while (*tzSym)
			pushbyte(expr, *tzSym++);
		pushbyte(expr, 0);
	} else {
		rpn_Number(expr, sym_GetConstantValue(tzSym));
	}
}

void rpn_BankSelf(struct Expression *expr)
{
	rpn_Init(expr);

	/*
	 * This symbol is not really relocatable, but this makes the assembler
	 * write this expression as a RPN patch to the object file.
	 */
	expr->isReloc = 1;

	pushbyte(expr, RPN_BANK_SELF);
}

void rpn_BankSymbol(struct Expression *expr, char *tzSym)
{
	/* The @ symbol is treated differently. */
	if (sym_FindSymbol(tzSym) == pPCSymbol) {
		rpn_BankSelf(expr);
		return;
	}

	if (!sym_isConstant(tzSym)) {
		rpn_Init(expr);
		sym_Ref(tzSym);
		expr->isReloc = 1;
		pushbyte(expr, RPN_BANK_SYM);
		while (*tzSym)
			pushbyte(expr, *tzSym++);
		pushbyte(expr, 0);
	} else {
		yyerror("BANK argument must be a relocatable identifier");
	}
}

void rpn_BankSection(struct Expression *expr, char *tzSectionName)
{
	rpn_Init(expr);

	/*
	 * This symbol is not really relocatable, but this makes the assembler
	 * write this expression as a RPN patch to the object file.
	 */
	expr->isReloc = 1;

	pushbyte(expr, RPN_BANK_SECT);
	while (*tzSectionName)
		pushbyte(expr, *tzSectionName++);
	pushbyte(expr, 0);
}

void rpn_CheckHRAM(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	pushbyte(expr, RPN_HRAM);
}

void rpn_LOGNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	pushbyte(expr, RPN_LOGUNNOT);
}

void rpn_LOGOR(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal || src2->nVal);
	pushbyte(expr, RPN_LOGOR);
}

void rpn_LOGAND(struct Expression *expr, const struct Expression *src1,
		const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal && src2->nVal);
	pushbyte(expr, RPN_LOGAND);
}

void rpn_HIGH(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;

	expr->nVal = (expr->nVal >> 8) & 0xFF;

	pushbyte(expr, RPN_CONST);
	pushbyte(expr, 8);
	pushbyte(expr, 0);
	pushbyte(expr, 0);
	pushbyte(expr, 0);

	pushbyte(expr, RPN_SHR);

	pushbyte(expr, RPN_CONST);
	pushbyte(expr, 0xFF);
	pushbyte(expr, 0);
	pushbyte(expr, 0);
	pushbyte(expr, 0);

	pushbyte(expr, RPN_AND);
}

void rpn_LOW(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;

	expr->nVal = expr->nVal & 0xFF;

	pushbyte(expr, RPN_CONST);
	pushbyte(expr, 0xFF);
	pushbyte(expr, 0);
	pushbyte(expr, 0);
	pushbyte(expr, 0);

	pushbyte(expr, RPN_AND);
}

void rpn_LOGEQU(struct Expression *expr, const struct Expression *src1,
		const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal == src2->nVal);
	pushbyte(expr, RPN_LOGEQ);
}

void rpn_LOGGT(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal > src2->nVal);
	pushbyte(expr, RPN_LOGGT);
}

void rpn_LOGLT(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal < src2->nVal);
	pushbyte(expr, RPN_LOGLT);
}

void rpn_LOGGE(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal >= src2->nVal);
	pushbyte(expr, RPN_LOGGE);
}

void rpn_LOGLE(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal <= src2->nVal);
	pushbyte(expr, RPN_LOGLE);
}

void rpn_LOGNE(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal != src2->nVal);
	pushbyte(expr, RPN_LOGNE);
}

void rpn_ADD(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal + src2->nVal);
	pushbyte(expr, RPN_ADD);
}

void rpn_SUB(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal - src2->nVal);
	pushbyte(expr, RPN_SUB);
}

void rpn_XOR(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal ^ src2->nVal);
	pushbyte(expr, RPN_XOR);
}

void rpn_OR(struct Expression *expr, const struct Expression *src1,
	    const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal | src2->nVal);
	pushbyte(expr, RPN_OR);
}

void rpn_AND(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal & src2->nVal);
	pushbyte(expr, RPN_AND);
}

void rpn_SHL(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();

	if (src1->nVal < 0)
		warning("Left shift of negative value: %d", src1->nVal);

	if (src2->nVal < 0)
		fatalerror("Shift by negative value: %d", src2->nVal);
	else if (src2->nVal >= 32)
		fatalerror("Shift by too big value: %d", src2->nVal);

	expr->nVal = (expr->nVal << src2->nVal);
	pushbyte(expr, RPN_SHL);
}

void rpn_SHR(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	if (src2->nVal < 0)
		fatalerror("Shift by negative value: %d", src2->nVal);
	else if (src2->nVal >= 32)
		fatalerror("Shift by too big value: %d", src2->nVal);

	expr->nVal = (expr->nVal >> src2->nVal);
	pushbyte(expr, RPN_SHR);
}

void rpn_MUL(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (expr->nVal * src2->nVal);
	pushbyte(expr, RPN_MUL);
}

void rpn_DIV(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	if (src2->nVal == 0)
		fatalerror("Division by zero");

	expr->nVal = (expr->nVal / src2->nVal);
	pushbyte(expr, RPN_DIV);
}

void rpn_MOD(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	if (src2->nVal == 0)
		fatalerror("Division by zero");

	expr->nVal = (expr->nVal % src2->nVal);
	pushbyte(expr, RPN_MOD);
}

void rpn_UNNEG(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = -expr->nVal;
	pushbyte(expr, RPN_UNSUB);
}

void rpn_UNNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = ~expr->nVal;
	pushbyte(expr, RPN_UNNOT);
}
