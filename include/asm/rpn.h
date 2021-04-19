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

struct Expression;

// Terminals (literals)
struct Expression *rpn_BankSymbol(char const *symName); // Also handles `BANK(@)`
struct Expression *rpn_BankSection(char const *sectionName);
struct Expression *rpn_SizeOfSection(char const *sectionName);
struct Expression *rpn_StartOfSection(char const *sectionName);
struct Expression *rpn_Number(uint32_t i);
struct Expression *rpn_Symbol(char const *symName);
// Unary operators
struct Expression *rpn_HIGH(struct Expression *expr); // Thin wrapper, never emitted in object files
struct Expression *rpn_LOW(struct Expression *expr); // Thin wrapper, never emitted in object files
struct Expression *rpn_UnaryOp(enum RPNCommand op, struct Expression *expr);
// Binary operators
struct Expression *rpn_BinaryOp(struct Expression *lhs, enum RPNCommand op, struct Expression *rhs);

struct RPNBuffer {
	uint32_t size;
	uint8_t buf[];
};

// Attempts to compute a RPN expression's value, and returns it.
// If it's impossible to, the function will optionally compute the coresponding RPN buffer
// and store a pointer to it in the supplied variable, while returning a dummy value (0).
// To be precise: if the supplied RPN buf ptr is NULL, then it's assumed that the caller requires
// a constant value. In this case, if it's impossible to compute one, an error is immediately
// produced, and a dummy value returned.
// If a RPN buffer is desired, it's also required to supply a callback to obtain symbol IDs
uint32_t rpn_Eval(struct Expression const *expr, struct RPNBuffer **rpnBufPtr);

// Check if the given expression is N-bit, and produce a warning otherwise
void rpn_CheckNBit(struct Expression const *expr, uint8_t n);

#endif /* RGBDS_ASM_RPN_H */
