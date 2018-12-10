/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_CONSTEXPR_H
#define RGBDS_ASM_CONSTEXPR_H

#include <stdint.h>

struct ConstExpression {
	union {
		int32_t nVal;
		struct sSymbol *pSym;
	} u;
	uint32_t isSym;
};

void constexpr_Symbol(struct ConstExpression *expr, char *tzSym);
void constexpr_Number(struct ConstExpression *expr, int32_t i);
void constexpr_UnaryOp(struct ConstExpression *expr,
		       int32_t op,
		       const struct ConstExpression *src);
void constexpr_BinaryOp(struct ConstExpression *expr,
			int32_t op,
			const struct ConstExpression *src1,
			const struct ConstExpression *src2);
int32_t constexpr_GetConstantValue(struct ConstExpression *expr);

#endif /* RGBDS_ASM_CONSTEXPR_H */
