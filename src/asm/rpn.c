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

	if (src1->nRPNLength + src2->nRPNLength > MAXRPNLEN)
		fatalerror("RPN expression is too large");

	uint32_t len = src1->nRPNLength + src2->nRPNLength;

	expr->nVal = 0;
	expr->tRPN = src1->tRPN;

	if (src1->nRPNCapacity >= len) {
		expr->nRPNCapacity = src1->nRPNCapacity;
	} else {
		uint32_t cap1 = src1->nRPNCapacity;
		uint32_t cap2 = src2->nRPNCapacity;
		uint32_t cap = (cap1 > cap2) ? cap1 : cap2;

		if (len > cap)
			cap = (cap <= MAXRPNLEN / 2) ? cap * 2 : MAXRPNLEN;

		expr->nRPNCapacity = cap;
		expr->tRPN = realloc(expr->tRPN, expr->nRPNCapacity);
		if (expr->tRPN == NULL)
			fatalerror("No memory for RPN expression");
	}

	memcpy(expr->tRPN + src1->nRPNLength, src2->tRPN, src2->nRPNLength);
	free(src2->tRPN);

	expr->nRPNLength = len;
	expr->nRPNPatchSize = src1->nRPNPatchSize + src2->nRPNPatchSize;
	expr->nRPNOut = 0;
	expr->isReloc = src1->isReloc || src2->isReloc;
}

#define joinexpr() mergetwoexpressions(expr, src1, src2)

/*
 * Add a byte to the RPN expression
 */
void pushbyte(struct Expression *expr, uint8_t b)
{
	if (expr->nRPNLength == expr->nRPNCapacity) {
		if (expr->nRPNCapacity == 0)
			expr->nRPNCapacity = 256;
		else if (expr->nRPNCapacity == MAXRPNLEN)
			fatalerror("RPN expression is too large");
		else if (expr->nRPNCapacity > MAXRPNLEN / 2)
			expr->nRPNCapacity = MAXRPNLEN;
		else
			expr->nRPNCapacity *= 2;
		expr->tRPN = realloc(expr->tRPN, expr->nRPNCapacity);

		if (expr->tRPN == NULL)
			fatalerror("No memory for RPN expression");
	}

	expr->tRPN[expr->nRPNLength++] = b;
}

/*
 * Init the RPN expression
 */
void rpn_Init(struct Expression *expr)
{
	expr->tRPN = NULL;
	expr->nRPNCapacity = 0;
	expr->nRPNLength = 0;
	expr->nRPNPatchSize = 0;
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
	expr->nRPNPatchSize += 5;
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
		expr->nRPNPatchSize += 5;
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
	expr->nRPNPatchSize++;
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
		expr->nRPNPatchSize += 5;
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
	expr->nRPNPatchSize++;

	while (*tzSectionName) {
		pushbyte(expr, *tzSectionName++);
		expr->nRPNPatchSize++;
	}

	pushbyte(expr, 0);
	expr->nRPNPatchSize++;
}

void rpn_CheckHRAM(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	pushbyte(expr, RPN_HRAM);
	expr->nRPNPatchSize++;
}

void rpn_LOGNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = !expr->nVal;
	pushbyte(expr, RPN_LOGUNNOT);
	expr->nRPNPatchSize++;
}

void rpn_LOGOR(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal || src2->nVal);
	pushbyte(expr, RPN_LOGOR);
	expr->nRPNPatchSize++;
}

void rpn_LOGAND(struct Expression *expr, const struct Expression *src1,
		const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal && src2->nVal);
	pushbyte(expr, RPN_LOGAND);
	expr->nRPNPatchSize++;
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

	expr->nRPNPatchSize += 12;
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

	expr->nRPNPatchSize += 6;
}

void rpn_LOGEQU(struct Expression *expr, const struct Expression *src1,
		const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal == src2->nVal);
	pushbyte(expr, RPN_LOGEQ);
	expr->nRPNPatchSize++;
}

void rpn_LOGGT(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal > src2->nVal);
	pushbyte(expr, RPN_LOGGT);
	expr->nRPNPatchSize++;
}

void rpn_LOGLT(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal < src2->nVal);
	pushbyte(expr, RPN_LOGLT);
	expr->nRPNPatchSize++;
}

void rpn_LOGGE(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal >= src2->nVal);
	pushbyte(expr, RPN_LOGGE);
	expr->nRPNPatchSize++;
}

void rpn_LOGLE(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal <= src2->nVal);
	pushbyte(expr, RPN_LOGLE);
	expr->nRPNPatchSize++;
}

void rpn_LOGNE(struct Expression *expr, const struct Expression *src1,
	       const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal != src2->nVal);
	pushbyte(expr, RPN_LOGNE);
	expr->nRPNPatchSize++;
}

void rpn_ADD(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = ((uint32_t)src1->nVal + (uint32_t)src2->nVal);
	pushbyte(expr, RPN_ADD);
	expr->nRPNPatchSize++;
}

void rpn_SUB(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = ((uint32_t)src1->nVal - (uint32_t)src2->nVal);
	pushbyte(expr, RPN_SUB);
	expr->nRPNPatchSize++;
}

void rpn_XOR(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal ^ src2->nVal);
	pushbyte(expr, RPN_XOR);
	expr->nRPNPatchSize++;
}

void rpn_OR(struct Expression *expr, const struct Expression *src1,
	    const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal | src2->nVal);
	pushbyte(expr, RPN_OR);
	expr->nRPNPatchSize++;
}

void rpn_AND(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = (src1->nVal & src2->nVal);
	pushbyte(expr, RPN_AND);
	expr->nRPNPatchSize++;
}

void rpn_SHL(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();

	if (!expr->isReloc) {
		if (src1->nVal < 0)
			warning("Left shift of negative value: %d", src1->nVal);

		if (src2->nVal < 0)
			fatalerror("Shift by negative value: %d", src2->nVal);
		else if (src2->nVal >= 32)
			fatalerror("Shift by too big value: %d", src2->nVal);

		expr->nVal = ((uint32_t)src1->nVal << src2->nVal);
	}

	pushbyte(expr, RPN_SHL);
	expr->nRPNPatchSize++;
}

void rpn_SHR(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();

	if (!expr->isReloc) {
		if (src2->nVal < 0)
			fatalerror("Shift by negative value: %d", src2->nVal);
		else if (src2->nVal >= 32)
			fatalerror("Shift by too big value: %d", src2->nVal);

		expr->nVal = (src1->nVal >> src2->nVal);
	}

	pushbyte(expr, RPN_SHR);
	expr->nRPNPatchSize++;
}

void rpn_MUL(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();
	expr->nVal = ((uint32_t)src1->nVal * (uint32_t)src2->nVal);
	pushbyte(expr, RPN_MUL);
	expr->nRPNPatchSize++;
}

void rpn_DIV(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();

	if (!expr->isReloc) {
		if (src2->nVal == 0)
			fatalerror("Division by zero");

		if (src1->nVal == INT32_MIN && src2->nVal == -1) {
			warning("Division of min value by -1");
			expr->nVal = INT32_MIN;
		} else {
			expr->nVal = (src1->nVal / src2->nVal);
		}
	}

	pushbyte(expr, RPN_DIV);
	expr->nRPNPatchSize++;
}

void rpn_MOD(struct Expression *expr, const struct Expression *src1,
	     const struct Expression *src2)
{
	joinexpr();

	if (!expr->isReloc) {
		if (src2->nVal == 0)
			fatalerror("Division by zero");

		if (src1->nVal == INT32_MIN && src2->nVal == -1)
			expr->nVal = 0;
		else
			expr->nVal = (src1->nVal % src2->nVal);
	}

	pushbyte(expr, RPN_MOD);
	expr->nRPNPatchSize++;
}

void rpn_UNNEG(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = -(uint32_t)expr->nVal;
	pushbyte(expr, RPN_UNSUB);
	expr->nRPNPatchSize++;
}

void rpn_UNNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = ~expr->nVal;
	pushbyte(expr, RPN_UNNOT);
	expr->nRPNPatchSize++;
}
