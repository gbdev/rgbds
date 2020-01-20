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
#include "asm/output.h"
#include "asm/warning.h"

/*
 * Add a byte to the RPN expression
 */
static void pushbyte(struct Expression *expr, uint8_t b)
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
	expr->isKnown = true;
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
 * Determine if the current expression is known at assembly time
 */
bool rpn_isKnown(const struct Expression *expr)
{
	return expr->isKnown;
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
	struct sSymbol *sym = sym_FindSymbol(tzSym);

	if (!sym || !sym_IsConstant(sym)) {
		rpn_Init(expr);
		sym_Ref(tzSym);
		expr->isKnown = false;
		pushbyte(expr, RPN_SYM);
		while (*tzSym)
			pushbyte(expr, *tzSym++);
		pushbyte(expr, 0);
		expr->nRPNPatchSize += 5;

		/* RGBLINK assumes PC is at the byte being computed... */
		if (sym == pPCSymbol && nPCOffset) {
			struct Expression pc = *expr, offset;

			rpn_Number(&offset, nPCOffset);
			rpn_BinaryOp(RPN_SUB, expr, &pc, &offset);
		}
	} else {
		rpn_Number(expr, sym_GetConstantValue(tzSym));
	}
}

void rpn_BankSelf(struct Expression *expr)
{
	rpn_Init(expr);

	if (pCurrentSection->nBank == -1)
		expr->isKnown = false;
	else
		expr->nVal = pCurrentSection->nBank;

	pushbyte(expr, RPN_BANK_SELF);
	expr->nRPNPatchSize++;
}

void rpn_BankSymbol(struct Expression *expr, char *tzSym)
{
	struct sSymbol const *sym = sym_FindSymbol(tzSym);

	/* The @ symbol is treated differently. */
	if (sym == pPCSymbol) {
		rpn_BankSelf(expr);
		return;
	}

	rpn_Init(expr);
	if (sym && sym_IsConstant(sym)) {
		yyerror("BANK argument must be a relocatable identifier");
	} else {
		sym_Ref(tzSym);
		pushbyte(expr, RPN_BANK_SYM);
		for (unsigned int i = 0; tzSym[i]; i++)
			pushbyte(expr, tzSym[i]);
		pushbyte(expr, 0);
		expr->nRPNPatchSize += 5;

		/* If the symbol didn't exist, `sym_Ref` created it */
		struct sSymbol *pSymbol = sym_FindSymbol(tzSym);

		if (pSymbol->pSection && pSymbol->pSection->nBank != -1)
			/* Symbol's section is known and bank's fixed */
			expr->nVal = pSymbol->pSection->nBank;
		else
			expr->isKnown = false;
	}
}

void rpn_BankSection(struct Expression *expr, char *tzSectionName)
{
	rpn_Init(expr);

	struct Section *pSection = out_FindSectionByName(tzSectionName);

	if (pSection && pSection->nBank != -1)
		expr->nVal = pSection->nBank;
	else
		expr->isKnown = false;

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

void rpn_CheckRST(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	pushbyte(expr, RPN_RST);
	expr->nRPNPatchSize++;
}

void rpn_LOGNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = !expr->nVal;
	pushbyte(expr, RPN_LOGUNNOT);
	expr->nRPNPatchSize++;
}

static int32_t shift(int32_t shiftee, int32_t amount)
{
	if (shiftee < 0)
		warning(WARNING_SHIFT, "Shifting negative value %d", shiftee);

	if (amount >= 0) {
		// Left shift
		if (amount >= 32) {
			warning(WARNING_SHIFT_AMOUNT, "Shifting left by large amount %d",
				amount);
			return 0;

		} else {
			/*
			 * Use unsigned to force a bitwise shift
			 * Casting back is OK because the types implement two's
			 * complement behavior
			 */
			return (uint32_t)shiftee << amount;
		}
	} else {
		// Right shift
		amount = -amount;
		if (amount >= 32) {
			warning(WARNING_SHIFT_AMOUNT, "Shifting right by large amount %d",
				amount);
			return shiftee < 0 ? -1 : 0;

		} else if (shiftee >= 0) {
			return shiftee >> amount;

		} else {
			/*
			 * The C standard leaves shifting right negative values
			 * undefined, so use a left shift manually sign-extended
			 */
			return (uint32_t)shiftee >> amount
				| -((uint32_t)1 << (32 - amount));
		}
	}
}

void rpn_BinaryOp(enum RPNCommand op, struct Expression *expr,
		  const struct Expression *src1, const struct Expression *src2)
{
	assert(src1->tRPN != NULL && src2->tRPN != NULL);

	uint32_t len = src1->nRPNLength + src2->nRPNLength;

	if (len > MAXRPNLEN)
		fatalerror("RPN expression is too large");

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
	expr->isKnown = src1->isKnown && src2->isKnown;

	switch (op) {
	case RPN_LOGOR:
		expr->nVal = src1->nVal || src2->nVal;
		break;
	case RPN_LOGAND:
		expr->nVal = src1->nVal && src2->nVal;
		break;
	case RPN_LOGEQ:
		expr->nVal = src1->nVal == src2->nVal;
		break;
	case RPN_LOGGT:
		expr->nVal = src1->nVal > src2->nVal;
		break;
	case RPN_LOGLT:
		expr->nVal = src1->nVal < src2->nVal;
		break;
	case RPN_LOGGE:
		expr->nVal = src1->nVal >= src2->nVal;
		break;
	case RPN_LOGLE:
		expr->nVal = src1->nVal <= src2->nVal;
		break;
	case RPN_LOGNE:
		expr->nVal = src1->nVal != src2->nVal;
		break;
	case RPN_ADD:
		expr->nVal = (uint32_t)src1->nVal + (uint32_t)src2->nVal;
		break;
	case RPN_SUB:
		expr->nVal = (uint32_t)src1->nVal - (uint32_t)src2->nVal;
		break;
	case RPN_XOR:
		expr->nVal = src1->nVal ^ src2->nVal;
		break;
	case RPN_OR:
		expr->nVal = src1->nVal | src2->nVal;
		break;
	case RPN_AND:
		expr->nVal = src1->nVal & src2->nVal;
		break;
	case RPN_SHL:
		if (expr->isKnown) {
			if (src2->nVal < 0)
				warning(WARNING_SHIFT_AMOUNT, "Shifting left by negative value: %d",
					src2->nVal);

			expr->nVal = shift(src1->nVal, src2->nVal);
		}
		break;
	case RPN_SHR:
		if (expr->isKnown) {
			if (src2->nVal < 0)
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by negative value: %d",
					src2->nVal);

			expr->nVal = shift(src1->nVal, -src2->nVal);
		}
		break;
	case RPN_MUL:
		expr->nVal = (uint32_t)src1->nVal * (uint32_t)src2->nVal;
		break;
	case RPN_DIV:
		if (expr->isKnown) {
			if (src2->nVal == 0)
				fatalerror("Division by zero");

			if (src1->nVal == INT32_MIN && src2->nVal == -1) {
				warning(WARNING_DIV, "Division of min value by -1");
				expr->nVal = INT32_MIN;
			} else {
				expr->nVal = src1->nVal / src2->nVal;
			}
		}
		break;
	case RPN_MOD:
		if (expr->isKnown) {
			if (src2->nVal == 0)
				fatalerror("Division by zero");

			if (src1->nVal == INT32_MIN && src2->nVal == -1)
				expr->nVal = 0;
			else
				expr->nVal = src1->nVal % src2->nVal;
		}
		break;

	case RPN_UNSUB:
	case RPN_UNNOT:
	case RPN_LOGUNNOT:
	case RPN_BANK_SYM:
	case RPN_BANK_SECT:
	case RPN_BANK_SELF:
	case RPN_HRAM:
	case RPN_RST:
	case RPN_CONST:
	case RPN_SYM:
		fatalerror("%d is no binary operator", op);
	}

	pushbyte(expr, op);
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
