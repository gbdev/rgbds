/*
 * Controls RPN expressions for objectfiles
 */

#include <stdio.h>
#include <string.h>

#include "asm/mylink.h"
#include "types.h"
#include "asm/symbol.h"
#include "asm/asm.h"
#include "asm/main.h"
#include "asm/rpn.h"

void 
mergetwoexpressions(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	*expr = *src1;
	memcpy(&(expr->tRPN[expr->nRPNLength]), src2->tRPN, src2->nRPNLength);

	expr->nRPNLength += src2->nRPNLength;
	expr->isReloc |= src2->isReloc;
	expr->isPCRel |= src2->isPCRel;
}
#define joinexpr() mergetwoexpressions(expr,src1,src2)

/*
 * Add a byte to the RPN expression
 */
void 
pushbyte(struct Expression * expr, int b)
{
	expr->tRPN[expr->nRPNLength++] = b & 0xFF;
}

/*
 * Reset the RPN module
 */
void 
rpn_Reset(struct Expression * expr)
{
	expr->nRPNLength = expr->nRPNOut = expr->isReloc = expr->isPCRel = 0;
}

/*
 * Returns the next rpn byte in expression
 */
UWORD 
rpn_PopByte(struct Expression * expr)
{
	if (expr->nRPNOut == expr->nRPNLength) {
		return (0xDEAD);
	} else
		return (expr->tRPN[expr->nRPNOut++]);
}

/*
 * Determine if the current expression is relocatable
 */
ULONG 
rpn_isReloc(struct Expression * expr)
{
	return (expr->isReloc);
}

/*
 * Determine if the current expression can be pc-relative
 */
ULONG 
rpn_isPCRelative(struct Expression * expr)
{
	return (expr->isPCRel);
}

/*
 * Add symbols, constants and operators to expression
 */
void 
rpn_Number(struct Expression * expr, ULONG i)
{
	rpn_Reset(expr);
	pushbyte(expr, RPN_CONST);
	pushbyte(expr, i);
	pushbyte(expr, i >> 8);
	pushbyte(expr, i >> 16);
	pushbyte(expr, i >> 24);
	expr->nVal = i;
}

void 
rpn_Symbol(struct Expression * expr, char *tzSym)
{
	if (!sym_isConstant(tzSym)) {
		struct sSymbol *psym;

		rpn_Reset(expr);

		psym = sym_FindSymbol(tzSym);

		if (psym == NULL || psym->pSection == pCurrentSection
		    || psym->pSection == NULL)
			expr->isPCRel = 1;
		expr->isReloc = 1;
		pushbyte(expr, RPN_SYM);
		while (*tzSym)
			pushbyte(expr, *tzSym++);
		pushbyte(expr, 0);
	} else
		rpn_Number(expr, sym_GetConstantValue(tzSym));
}

void 
rpn_Bank(struct Expression * expr, char *tzSym)
{
	if (!sym_isConstant(tzSym)) {
		rpn_Reset(expr);

		/* Check that the symbol exists by evaluating and discarding the value. */
		sym_GetValue(tzSym);

		expr->isReloc = 1;
		pushbyte(expr, RPN_BANK);
		while (*tzSym)
			pushbyte(expr, *tzSym++);
		pushbyte(expr, 0);
	} else
		yyerror("BANK argument must be a relocatable identifier");
}

int 
rpn_RangeCheck(struct Expression * expr, struct Expression * src, SLONG low,
    SLONG high)
{
	*expr = *src;

	if (rpn_isReloc(src)) {
		pushbyte(expr, RPN_RANGECHECK);
		pushbyte(expr, low);
		pushbyte(expr, low >> 8);
		pushbyte(expr, low >> 16);
		pushbyte(expr, low >> 24);
		pushbyte(expr, high);
		pushbyte(expr, high >> 8);
		pushbyte(expr, high >> 16);
		pushbyte(expr, high >> 24);
		return (1);
	} else {
		return (expr->nVal >= low && expr->nVal <= high);
	}
}

void 
rpn_CheckHRAM(struct Expression * expr, struct Expression * src)
{
	*expr = *src;
	pushbyte(expr, RPN_HRAM);
}

void 
rpn_LOGNOT(struct Expression * expr, struct Expression * src)
{
	*expr = *src;
	pushbyte(expr, RPN_LOGUNNOT);
}

void 
rpn_LOGOR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal || src2->nVal);
	pushbyte(expr, RPN_LOGOR);
}

void 
rpn_LOGAND(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal && src2->nVal);
	pushbyte(expr, RPN_LOGAND);
}

void 
rpn_LOGEQU(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal == src2->nVal);
	pushbyte(expr, RPN_LOGEQ);
}

void 
rpn_LOGGT(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal > src2->nVal);
	pushbyte(expr, RPN_LOGGT);
}

void 
rpn_LOGLT(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal < src2->nVal);
	pushbyte(expr, RPN_LOGLT);
}

void 
rpn_LOGGE(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal >= src2->nVal);
	pushbyte(expr, RPN_LOGGE);
}

void 
rpn_LOGLE(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal <= src2->nVal);
	pushbyte(expr, RPN_LOGLE);
}

void 
rpn_LOGNE(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal != src2->nVal);
	pushbyte(expr, RPN_LOGNE);
}

void 
rpn_ADD(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal + src2->nVal);
	pushbyte(expr, RPN_ADD);
}

void 
rpn_SUB(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal - src2->nVal);
	pushbyte(expr, RPN_SUB);
}

void 
rpn_XOR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal ^ src2->nVal);
	pushbyte(expr, RPN_XOR);
}

void 
rpn_OR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal | src2->nVal);
	pushbyte(expr, RPN_OR);
}

void 
rpn_AND(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal & src2->nVal);
	pushbyte(expr, RPN_AND);
}

void 
rpn_SHL(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal << src2->nVal);
	pushbyte(expr, RPN_SHL);
}

void 
rpn_SHR(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal >> src2->nVal);
	pushbyte(expr, RPN_SHR);
}

void 
rpn_MUL(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	expr->nVal = (expr->nVal * src2->nVal);
	pushbyte(expr, RPN_MUL);
}

void 
rpn_DIV(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	if (src2->nVal == 0) {
		fatalerror("division by zero");
	}
	expr->nVal = (expr->nVal / src2->nVal);
	pushbyte(expr, RPN_DIV);
}

void 
rpn_MOD(struct Expression * expr, struct Expression * src1,
    struct Expression * src2)
{
	joinexpr();
	if (src2->nVal == 0) {
		fatalerror("division by zero");
	}
	expr->nVal = (expr->nVal % src2->nVal);
	pushbyte(expr, RPN_MOD);
}

void 
rpn_UNNEG(struct Expression * expr, struct Expression * src)
{
	*expr = *src;
	expr->nVal = -expr->nVal;
	pushbyte(expr, RPN_UNSUB);
}

void 
rpn_UNNOT(struct Expression * expr, struct Expression * src)
{
	*expr = *src;
	expr->nVal = expr->nVal ^ 0xFFFFFFFF;
	pushbyte(expr, RPN_UNNOT);
}
