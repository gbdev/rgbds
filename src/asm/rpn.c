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
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/symbol.h"
#include "asm/output.h"
#include "asm/warning.h"

static uint8_t *reserveSpace(struct Expression *expr, uint32_t size)
{
	/* This assumes the RPN length is always less than the capacity */
	if (expr->nRPNCapacity - expr->nRPNLength < size) {
		/* If there isn't enough room to reserve the space, realloc */
		if (!expr->tRPN)
			expr->nRPNCapacity = 256; /* Initial size */
		else if (expr->nRPNCapacity >= MAXRPNLEN)
			/*
			 * To avoid generating humongous object files, cap the
			 * size of RPN expressions
			 */
			fatalerror("RPN expression cannot grow larger than %d bytes",
				   MAXRPNLEN);
		else if (expr->nRPNCapacity > MAXRPNLEN / 2)
			expr->nRPNCapacity = MAXRPNLEN;
		else
			expr->nRPNCapacity *= 2;
		expr->tRPN = realloc(expr->tRPN, expr->nRPNCapacity);

		if (!expr->tRPN)
			fatalerror("Failed to grow RPN expression: %s",
				   strerror(errno));
	}

	uint8_t *ptr = expr->tRPN + expr->nRPNLength;

	expr->nRPNLength += size;
	return ptr;
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
	uint8_t bytes[] = {RPN_CONST, i, i >> 8, i >> 16, i >> 24};

	rpn_Init(expr);
	expr->nRPNPatchSize += sizeof(bytes);
	memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
	expr->nVal = i;
}

void rpn_Symbol(struct Expression *expr, char *tzSym)
{
	struct sSymbol *sym = sym_FindSymbol(tzSym);

	if (!sym || !sym_IsConstant(sym)) {
		rpn_Init(expr);
		sym_Ref(tzSym);
		expr->isKnown = false;
		expr->nRPNPatchSize += 5; /* 1-byte opcode + 4-byte symbol ID */

		size_t nameLen = strlen(tzSym) + 1; /* Don't forget NUL! */
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);
		*ptr++ = RPN_SYM;
		memcpy(ptr, tzSym, nameLen);

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

	*reserveSpace(expr, 1) = RPN_BANK_SELF;
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
		expr->nRPNPatchSize += 5; /* 1-byte opcode + 4-byte sect ID */

		size_t nameLen = strlen(tzSym) + 1; /* Don't forget NUL! */
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);
		*ptr++ = RPN_BANK_SYM;
		memcpy(ptr, tzSym, nameLen);

		/* If the symbol didn't exist, `sym_Ref` created it */
		struct sSymbol *pSymbol = sym_FindSymbol(tzSym);

		if (pSymbol->pSection && pSymbol->pSection->nBank != -1)
			/* Symbol's section is known and bank is fixed */
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

	size_t nameLen = strlen(tzSectionName) + 1; /* Don't forget NUL! */
	uint8_t *ptr = reserveSpace(expr, nameLen + 1);

	expr->nRPNPatchSize += nameLen + 1;
	*ptr++ = RPN_BANK_SECT;
	memcpy(ptr, tzSectionName, nameLen);
}

void rpn_CheckHRAM(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nRPNPatchSize++;
	*reserveSpace(expr, 1) = RPN_HRAM;
}

void rpn_CheckRST(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	*reserveSpace(expr, 1) = RPN_RST;
	expr->nRPNPatchSize++;
}

void rpn_LOGNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = !expr->nVal;
	expr->nRPNPatchSize++;
	*reserveSpace(expr, 1) = RPN_LOGUNNOT;
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

	expr->nRPNOut = 0; // FIXME: is this necessary?
	expr->isKnown = src1->isKnown && src2->isKnown;
	expr->nRPNLength = len;
	expr->nRPNPatchSize = src1->nRPNPatchSize + src2->nRPNPatchSize + 1;
	*reserveSpace(expr, 1) = op;

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
		// FIXME: under certain conditions, this might be actually known
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
}

void rpn_HIGH(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;

	expr->nVal = (expr->nVal >> 8) & 0xFF;

	uint8_t bytes[] = {RPN_CONST,    8, 0, 0, 0, RPN_SHR,
			   RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};
	expr->nRPNPatchSize += sizeof(bytes);

	memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
}

void rpn_LOW(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;

	expr->nVal = expr->nVal & 0xFF;

	uint8_t bytes[] = {RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

	expr->nRPNPatchSize += sizeof(bytes);
	memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
}

void rpn_UNNEG(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = -(uint32_t)expr->nVal;
	expr->nRPNPatchSize++;
	*reserveSpace(expr, 1) = RPN_UNSUB;
}

void rpn_UNNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->nVal = ~expr->nVal;
	expr->nRPNPatchSize++;
	*reserveSpace(expr, 1) = RPN_UNNOT;
}
