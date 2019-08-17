/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/constexpr.h"
#include "asm/lexer.h"
#include "asm/main.h"
#include "asm/mymath.h"
#include "asm/rpn.h"
#include "asm/symbol.h"

#include "asmy.h"

void constexpr_Symbol(struct ConstExpression *expr, char *tzSym)
{
	if (!sym_isConstant(tzSym)) {
		struct sSymbol *pSym = sym_FindSymbol(tzSym);

		if (pSym != NULL) {
			expr->u.pSym = pSym;
			expr->isSym = 1;
		} else {
			fatalerror("'%s' not defined", tzSym);
		}
	} else {
		constexpr_Number(expr, sym_GetConstantValue(tzSym));
	}
}

void constexpr_Number(struct ConstExpression *expr, int32_t i)
{
	expr->u.nVal = i;
	expr->isSym = 0;
}

void constexpr_UnaryOp(struct ConstExpression *expr,
		       int32_t op,
		       const struct ConstExpression *src)
{
	if (src->isSym)
		fatalerror("Non-constant operand in constant expression");

	int32_t value = src->u.nVal;
	int32_t result = 0;

	switch (op) {
	case T_OP_HIGH:
		result = (value >> 8) & 0xFF;
		break;
	case T_OP_LOW:
		result = value & 0xFF;
		break;
	case T_OP_LOGICNOT:
		result = !value;
		break;
	case T_OP_ADD:
		result = value;
		break;
	case T_OP_SUB:
		result = -(uint32_t)value;
		break;
	case T_OP_NOT:
		result = ~value;
		break;
	case T_OP_ROUND:
		result = math_Round(value);
		break;
	case T_OP_CEIL:
		result = math_Ceil(value);
		break;
	case T_OP_FLOOR:
		result = math_Floor(value);
		break;
	case T_OP_SIN:
		result = math_Sin(value);
		break;
	case T_OP_COS:
		result = math_Cos(value);
		break;
	case T_OP_TAN:
		result = math_Tan(value);
		break;
	case T_OP_ASIN:
		result = math_ASin(value);
		break;
	case T_OP_ACOS:
		result = math_ACos(value);
		break;
	case T_OP_ATAN:
		result = math_ATan(value);
		break;
	default:
		fatalerror("Unknown unary op");
	}

	constexpr_Number(expr, result);
}

void constexpr_BinaryOp(struct ConstExpression *expr,
			int32_t op,
			const struct ConstExpression *src1,
			const struct ConstExpression *src2)
{
	int32_t value1;
	int32_t value2;
	int32_t result = 0;

	if (op == T_OP_SUB && src1->isSym && src2->isSym) {
		char *symName1 = src1->u.pSym->tzName;
		char *symName2 = src2->u.pSym->tzName;

		if (!sym_IsRelocDiffDefined(symName1, symName2))
			fatalerror("'%s - %s' not defined", symName1, symName2);
		value1 = sym_GetDefinedValue(symName1);
		value2 = sym_GetDefinedValue(symName2);
		result = value1 - value2;
	} else if (src1->isSym || src2->isSym) {
		fatalerror("Non-constant operand in constant expression");
	} else {
		value1 = src1->u.nVal;
		value2 = src2->u.nVal;

		switch (op) {
		case T_OP_LOGICOR:
			result = value1 || value2;
			break;
		case T_OP_LOGICAND:
			result = value1 && value2;
			break;
		case T_OP_LOGICEQU:
			result = value1 == value2;
			break;
		case T_OP_LOGICGT:
			result = value1 > value2;
			break;
		case T_OP_LOGICLT:
			result = value1 < value2;
			break;
		case T_OP_LOGICGE:
			result = value1 >= value2;
			break;
		case T_OP_LOGICLE:
			result = value1 <= value2;
			break;
		case T_OP_LOGICNE:
			result = value1 != value2;
			break;
		case T_OP_ADD:
			result = (uint32_t)value1 + (uint32_t)value2;
			break;
		case T_OP_SUB:
			result = (uint32_t)value1 - (uint32_t)value2;
			break;
		case T_OP_XOR:
			result = value1 ^ value2;
			break;
		case T_OP_OR:
			result = value1 | value2;
			break;
		case T_OP_AND:
			result = value1 & value2;
			break;
		case T_OP_SHL:
			if (value1 < 0)
				warning("Left shift of negative value: %d",
					value1);

			if (value2 < 0)
				fatalerror("Shift by negative value: %d",
					   value2);
			else if (value2 >= 32)
				fatalerror("Shift by too big value: %d",
					   value2);

			result = (uint32_t)value1 << value2;
			break;
		case T_OP_SHR:
			if (value2 < 0)
				fatalerror("Shift by negative value: %d",
					   value2);
			else if (value2 >= 32)
				fatalerror("Shift by too big value: %d",
					   value2);

			result = value1 >> value2;
			break;
		case T_OP_MUL:
			result = (uint32_t)value1 * (uint32_t)value2;
			break;
		case T_OP_DIV:
			if (value2 == 0)
				fatalerror("Division by zero");
			if (value1 == INT32_MIN && value2 == -1) {
				warning("Division of min value by -1");
				result = INT32_MIN;
			} else {
				result = value1 / value2;
			}
			break;
		case T_OP_MOD:
			if (value2 == 0)
				fatalerror("Division by zero");
			if (value1 == INT32_MIN && value2 == -1)
				result = 0;
			else
				result = value1 % value2;
			break;
		case T_OP_FDIV:
			result = math_Div(value1, value2);
			break;
		case T_OP_FMUL:
			result = math_Mul(value1, value2);
			break;
		case T_OP_ATAN2:
			result = math_ATan2(value1, value2);
			break;
		default:
			fatalerror("Unknown binary op");
		}
	}

	constexpr_Number(expr, result);
}

int32_t constexpr_GetConstantValue(struct ConstExpression *expr)
{
	if (expr->isSym)
		fatalerror("Non-constant expression");
	return expr->u.nVal;
}
