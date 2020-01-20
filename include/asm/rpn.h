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
	int32_t  nVal;
	uint8_t  *tRPN;
	uint32_t nRPNCapacity;
	uint32_t nRPNLength;
	uint32_t nRPNPatchSize;
	uint32_t nRPNOut;
	bool isKnown;
};

/* FIXME: Should be defined in `asmy.h`, but impossible with POSIX Yacc */
extern int32_t nPCOffset;

bool rpn_isKnown(const struct Expression *expr);
void rpn_Symbol(struct Expression *expr, char *tzSym);
void rpn_Number(struct Expression *expr, uint32_t i);
void rpn_LOGNOT(struct Expression *expr, const struct Expression *src);
void rpn_BinaryOp(enum RPNCommand op, struct Expression *expr,
		  const struct Expression *src1,
		  const struct Expression *src2);
void rpn_HIGH(struct Expression *expr, const struct Expression *src);
void rpn_LOW(struct Expression *expr, const struct Expression *src);
void rpn_UNNEG(struct Expression *expr, const struct Expression *src);
void rpn_UNNOT(struct Expression *expr, const struct Expression *src);
uint16_t rpn_PopByte(struct Expression *expr);
void rpn_BankSymbol(struct Expression *expr, char *tzSym);
void rpn_BankSection(struct Expression *expr, char *tzSectionName);
void rpn_BankSelf(struct Expression *expr);
void rpn_Init(struct Expression *expr);
void rpn_Free(struct Expression *expr);
void rpn_CheckHRAM(struct Expression *expr, const struct Expression *src);
void rpn_CheckRST(struct Expression *expr, const struct Expression *src);

#endif /* RGBDS_ASM_RPN_H */
