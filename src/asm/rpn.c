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
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/warning.h"

/* Makes an expression "not known", also setting its error message */
#define makeUnknown(expr_, ...) do { \
	struct Expression *_expr = expr_; \
	_expr->isKnown = false; \
	/* If we had `asprintf` this would be great, but alas. */ \
	_expr->reason = malloc(128); /* Use an initial reasonable size */ \
	if (!_expr->reason) \
		fatalerror("Can't allocate err string: %s", strerror(errno)); \
	int size = snprintf(_expr->reason, 128, __VA_ARGS__); \
	if (size >= 128) { /* If this wasn't enough, try again */ \
		_expr->reason = realloc(_expr->reason, size + 1); \
		sprintf(_expr->reason, __VA_ARGS__); \
	} \
} while (0)

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
	expr->reason = NULL;
	expr->isKnown = true;
	expr->isSymbol = false;
	expr->tRPN = NULL;
	expr->nRPNCapacity = 0;
	expr->nRPNLength = 0;
	expr->nRPNPatchSize = 0;
	expr->nRPNOut = 0;
}

/*
 * Free the RPN expression
 */
void rpn_Free(struct Expression *expr)
{
	free(expr->tRPN);
	free(expr->reason);
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
 * Determine if the current expression is a symbol suitable for const diffing
 */
bool rpn_isSymbol(const struct Expression *expr)
{
	return expr->isSymbol;
}

/*
 * Add symbols, constants and operators to expression
 */
void rpn_Number(struct Expression *expr, uint32_t i)
{
	rpn_Init(expr);
	expr->nVal = i;
}

void rpn_Symbol(struct Expression *expr, char *tzSym)
{
	struct sSymbol *sym = sym_FindSymbol(tzSym);

	if (!sym || !sym_IsConstant(sym)) {
		rpn_Init(expr);
		expr->isSymbol = true;

		sym_Ref(tzSym);
		makeUnknown(expr, strcmp(tzSym, "@")
				      ? "'%s' is not constant at assembly time"
				      : "PC is not constant at assembly time",
			    tzSym);
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
			if (!rpn_isKnown(expr))
				expr->isSymbol = true;
		}
	} else {
		rpn_Number(expr, sym_GetConstantValue(tzSym));
	}
}

void rpn_BankSelf(struct Expression *expr)
{
	rpn_Init(expr);

	if (pCurrentSection->nBank == -1) {
		makeUnknown(expr, "Current section's bank is not known");
		expr->nRPNPatchSize++;
		*reserveSpace(expr, 1) = RPN_BANK_SELF;
	} else {
		expr->nVal = pCurrentSection->nBank;
	}
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
		/* If the symbol didn't exist, `sym_Ref` created it */
		struct sSymbol *pSymbol = sym_FindSymbol(tzSym);

		if (pSymbol->pSection && pSymbol->pSection->nBank != -1) {
			/* Symbol's section is known and bank is fixed */
			expr->nVal = pSymbol->pSection->nBank;
		} else {
			makeUnknown(expr, "\"%s\"'s bank is not known", tzSym);
			expr->nRPNPatchSize += 5; /* opcode + 4-byte sect ID */

			size_t nameLen = strlen(tzSym) + 1; /* Room for NUL! */
			uint8_t *ptr = reserveSpace(expr, nameLen + 1);
			*ptr++ = RPN_BANK_SYM;
			memcpy(ptr, tzSym, nameLen);
		}
	}
}

void rpn_BankSection(struct Expression *expr, char *tzSectionName)
{
	rpn_Init(expr);

	struct Section *pSection = out_FindSectionByName(tzSectionName);

	if (pSection && pSection->nBank != -1) {
		expr->nVal = pSection->nBank;
	} else {
		makeUnknown(expr, "Section \"%s\"'s bank is not known",
			    tzSectionName);

		size_t nameLen = strlen(tzSectionName) + 1; /* Room for NUL! */
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);

		expr->nRPNPatchSize += nameLen + 1;
		*ptr++ = RPN_BANK_SECT;
		memcpy(ptr, tzSectionName, nameLen);
	}
}

void rpn_CheckHRAM(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->isSymbol = false;

	if (rpn_isKnown(expr)) {
		/* TODO */
	} else {
		expr->nRPNPatchSize++;
		*reserveSpace(expr, 1) = RPN_HRAM;
	}
}

void rpn_CheckRST(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;

	if (rpn_isKnown(expr)) {
		/* TODO */
	} else {
		expr->nRPNPatchSize++;
		*reserveSpace(expr, 1) = RPN_RST;
	}
}

void rpn_LOGNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->isSymbol = false;

	if (rpn_isKnown(expr)) {
		expr->nVal = !expr->nVal;
	} else {
		expr->nRPNPatchSize++;
		*reserveSpace(expr, 1) = RPN_LOGUNNOT;
	}
}

static int32_t shift(int32_t shiftee, int32_t amount)
{
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

static struct sSymbol const *symbolOf(struct Expression const *expr)
{
	if (!rpn_isSymbol(expr))
		return NULL;
	return sym_FindSymbol((char *)expr->tRPN + 1);
}

static bool isDiffConstant(struct Expression const *src1,
			   struct Expression const *src2)
{
	/* Check if both expressions only refer to a single symbol */
	struct sSymbol const *symbol1 = symbolOf(src1);
	struct sSymbol const *symbol2 = symbolOf(src2);

	if (!symbol1 || !symbol2
	 || symbol1->type != SYM_LABEL || symbol2->type != SYM_LABEL)
		return false;

	return symbol1->pSection == symbol2->pSection;
}

void rpn_BinaryOp(enum RPNCommand op, struct Expression *expr,
		  const struct Expression *src1, const struct Expression *src2)
{
	expr->isSymbol = false;

	/* First, check if the expression is known */
	expr->isKnown = src1->isKnown && src2->isKnown;
	if (expr->isKnown) {
		rpn_Init(expr); /* Init the expression to something sane */

		/* If both expressions are known, just compute the value */
		uint32_t uleft = src1->nVal, uright = src2->nVal;

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
			expr->nVal = uleft + uright;
			break;
		case RPN_SUB:
			expr->nVal = uleft - uright;
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
			if (src2->nVal < 0)
				warning(WARNING_SHIFT_AMOUNT, "Shifting left by negative amount %d",
					src2->nVal);

			expr->nVal = shift(src1->nVal, src2->nVal);
			break;
		case RPN_SHR:
			if (src1->nVal < 0)
				warning(WARNING_SHIFT, "Shifting negative value %d",
					src1->nVal);

			if (src2->nVal < 0)
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %d",
					src2->nVal);

			expr->nVal = shift(src1->nVal, -src2->nVal);
			break;
		case RPN_MUL:
			expr->nVal = uleft * uright;
			break;
		case RPN_DIV:
			if (src2->nVal == 0)
				fatalerror("Division by zero");

			if (src1->nVal == INT32_MIN
			 && src2->nVal == -1) {
				warning(WARNING_DIV, "Division of min value by -1");
				expr->nVal = INT32_MIN;
			} else {
				expr->nVal = src1->nVal / src2->nVal;
			}
			break;
		case RPN_MOD:
			if (src2->nVal == 0)
				fatalerror("Division by zero");

			if (src1->nVal == INT32_MIN && src2->nVal == -1)
				expr->nVal = 0;
			else
				expr->nVal = src1->nVal % src2->nVal;
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

	} else if (op == RPN_SUB && isDiffConstant(src1, src2)) {
		struct sSymbol const *symbol1 = symbolOf(src1);
		struct sSymbol const *symbol2 = symbolOf(src2);

		expr->nVal = sym_GetValue(symbol1) - sym_GetValue(symbol2);
		expr->isKnown = true;
	} else {
		/* If it's not known, start computing the RPN expression */

		/* Convert the left-hand expression if it's constant */
		if (src1->isKnown) {
			uint32_t lval = src1->nVal;
			uint8_t bytes[] = {RPN_CONST, lval, lval >> 8,
					   lval >> 16, lval >> 24};
			expr->nRPNPatchSize = sizeof(bytes);
			expr->tRPN = NULL;
			expr->nRPNCapacity = 0;
			expr->nRPNLength = 0;
			memcpy(reserveSpace(expr, sizeof(bytes)), bytes,
			       sizeof(bytes));

			/* Use the other expression's un-const reason */
			expr->reason = src2->reason;
			free(src1->reason);
		} else {
			/* Otherwise just reuse its RPN buffer */
			expr->nRPNPatchSize = src1->nRPNPatchSize;
			expr->tRPN = src1->tRPN;
			expr->nRPNCapacity = src1->nRPNCapacity;
			expr->nRPNLength = src1->nRPNLength;
			expr->reason = src1->reason;
			free(src2->reason);
		}

		/* Now, merge the right expression into the left one */
		uint8_t *ptr = src2->tRPN; /* Pointer to the right RPN */
		uint32_t len = src2->nRPNLength; /* Size of the right RPN */
		uint32_t patchSize = src2->nRPNPatchSize;

		/* If the right expression is constant, merge a shim instead */
		uint32_t rval = src2->nVal;
		uint8_t bytes[] = {RPN_CONST, rval, rval >> 8, rval >> 16,
				   rval >> 24};
		if (src2->isKnown) {
			ptr = bytes;
			len = sizeof(bytes);
			patchSize = sizeof(bytes);
		}
		/* Copy the right RPN and append the operator */
		uint8_t *buf = reserveSpace(expr, len + 1);

		memcpy(buf, ptr, len);
		buf[len] = op;

		free(src2->tRPN); /* If there was none, this is `free(NULL)` */
		expr->nRPNPatchSize += patchSize + 1;
	}
}

void rpn_HIGH(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->isSymbol = false;

	if (rpn_isKnown(expr)) {
		expr->nVal = (uint32_t)expr->nVal >> 8 & 0xFF;
	} else {
		uint8_t bytes[] = {RPN_CONST,    8, 0, 0, 0, RPN_SHR,
				   RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};
		expr->nRPNPatchSize += sizeof(bytes);
		memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
	}
}

void rpn_LOW(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->isSymbol = false;

	if (rpn_isKnown(expr)) {
		expr->nVal = expr->nVal & 0xFF;
	} else {
		uint8_t bytes[] = {RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

		expr->nRPNPatchSize += sizeof(bytes);
		memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
	}
}

void rpn_UNNEG(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->isSymbol = false;

	if (rpn_isKnown(expr)) {
		expr->nVal = -(uint32_t)expr->nVal;
	} else {
		expr->nRPNPatchSize++;
		*reserveSpace(expr, 1) = RPN_UNSUB;
	}
}

void rpn_UNNOT(struct Expression *expr, const struct Expression *src)
{
	*expr = *src;
	expr->isSymbol = false;

	if (rpn_isKnown(expr)) {
		expr->nVal = ~expr->nVal;
	} else {
		expr->nRPNPatchSize++;
		*reserveSpace(expr, 1) = RPN_UNNOT;
	}
}
