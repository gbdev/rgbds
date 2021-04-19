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
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/main.h"
#include "asm/output.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/symbol.h"
#include "asm/warning.h"

#include "opmath.h"
#include "rpn.h"

struct Expression {
	size_t   rpnLength; // Used size of the `rpn` buffer
	uint8_t  rpn[];     // Array of bytes serializing the RPN expression (see rgbds(5) roughly)
};

/*
 * Determines if an expression is known at assembly time
 */
static bool isConstant(struct Expression const *expr)
{
	// An expression is known if it reduces to a single number
	return expr->rpnLength == 1 + sizeof(uint32_t) && expr->rpn[0] == RPN_CONST;
}

static uint32_t getConstVal(struct Expression const *expr)
{
	return readLE32(&expr->rpn[1]);
}

static void setConstVal(struct Expression *expr, uint32_t i)
{
	writeLE32(&expr->rpn[1], i);
}

/*
 * Init a RPN expression
 */
static struct Expression *rpn_Init(size_t rpnSize)
{
	// FIXME: there's a possible overflow there
	struct Expression *expr = malloc(sizeof(*expr) + rpnSize);

	if (!expr)
		fatalerror("Failed to alloc expression: %s\n", strerror(errno));
	expr->rpnLength = rpnSize;
	// Don't init the RPN buffer, the caller will do it
	return expr;
}

static struct Expression *rpn_Grow(struct Expression *expr, size_t additionalSize)
{
	// FIXME: there's a possible overflow here
	expr->rpnLength += additionalSize;
	expr = realloc(expr, sizeof(*expr) + expr->rpnLength);
	if (!expr)
		fatalerror("Failed to grow expression: %s\n", strerror(errno));
	return expr;
}

/// TERMINALS

struct Expression *rpn_Number(uint32_t i)
{
	struct Expression *expr = rpn_Init(1 + sizeof(uint32_t));

	expr->rpn[0] = RPN_CONST;
	setConstVal(expr, i);
	return expr;
}

struct Expression *rpn_Symbol(char const *symName)
{
	// FIXME: we have two possible overflows here
	size_t nameLen = strlen(symName) + 1; // Don't forget the terminator!
	struct Expression *expr = rpn_Init(1 + nameLen);

	expr->rpn[0] = RPN_SYM;
	memcpy(&expr->rpn[1], symName, nameLen);
	return expr;
}

static struct Expression *rpn_BankSelf(void)
{
	struct Expression *expr = rpn_Init(1);

	expr->rpn[0] = RPN_BANK_SELF;
	return expr;
}

struct Expression *rpn_BankSymbol(char const *symName)
{
	struct Symbol const *sym = sym_FindScopedSymbol(symName);

	if (sym_IsPC(sym)) // This can be determined at parsing time, no need to defer to evaluation
		return rpn_BankSelf();

	// FIXME: we have two possible overflows here
	size_t nameLen = strlen(symName) + 1; // Don't forget the terminator!
	struct Expression *expr = rpn_Init(1 + nameLen);

	expr->rpn[0] = RPN_BANK_SYM;
	memcpy(&expr->rpn[1], symName, nameLen);
	return expr;
}

struct Expression *rpn_BankSection(char const *sectionName)
{
	// FIXME: we have two possible overflows here
	size_t nameLen = strlen(sectionName) + 1; // Don't forget the terminator!
	struct Expression *expr = rpn_Init(1 + nameLen);

	expr->rpn[0] = RPN_BANK_SECT;
	memcpy(&expr->rpn[1], sectionName, nameLen);
	return expr;
}

struct Expression *rpn_SizeOfSection(char const *sectionName)
{
	// FIXME: we have two possible overflows here
	size_t nameLen = strlen(sectionName) + 1; // Don't forget the terminator!
	struct Expression *expr = rpn_Init(1 + nameLen);

	expr->rpn[0] = RPN_SIZEOF_SECT;
	memcpy(&expr->rpn[1], sectionName, nameLen);
	return expr;
}

struct Expression *rpn_StartOfSection(char const *sectionName)
{
	// FIXME: we have two possible overflows here
	size_t nameLen = strlen(sectionName) + 1; // Don't forget the terminator!
	struct Expression *expr = rpn_Init(1 + nameLen);

	expr->rpn[0] = RPN_STARTOF_SECT;
	memcpy(&expr->rpn[1], sectionName, nameLen);
	return expr;
}

/// UNARY OPERATORS

struct Expression *rpn_HIGH(struct Expression *expr)
{
	if (isConstant(expr)) {
		// Truncate the result to a single byte via unsigned casting
		uint8_t val = getConstVal(expr) >> 8;

		setConstVal(expr, val);
		return expr;
	}

	static uint8_t bytes[] = {RPN_CONST,    8, 0, 0, 0, RPN_SHR,
				  RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

	expr = rpn_Grow(expr, sizeof(bytes));
	memcpy(&expr->rpn[expr->rpnLength - sizeof(bytes)], bytes, sizeof(bytes));
	return expr;
}

struct Expression *rpn_LOW(struct Expression *expr)
{
	if (isConstant(expr)) {
		// Truncate the result to a single byte via unsigned casting
		uint8_t val = getConstVal(expr);

		setConstVal(expr, val);
		return expr;
	}

	static uint8_t bytes[] = {RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

	expr = rpn_Grow(expr, sizeof(bytes));
	memcpy(&expr->rpn[expr->rpnLength - sizeof(bytes)], bytes, sizeof(bytes));
	return expr;
}

struct Expression *rpn_UnaryOp(enum RPNCommand op, struct Expression *expr)
{
	if (isConstant(expr)) {
		setConstVal(expr, rpn_ConstUnaryOp(op, getConstVal(expr), NULL));
		return expr;
	}

	expr = rpn_Grow(expr, 1);
	expr->rpn[expr->rpnLength - 1] = op;
	return expr;
}

/// BINARY OPERATORS

struct Expression *rpn_BinaryOp(struct Expression *lhs, enum RPNCommand op, struct Expression *rhs)
{
	// Modify `lhs` to contain the result, and free `rhs`

	if (isConstant(lhs)) {
		uint32_t lhsVal = getConstVal(lhs);

		// We might have a chance with short-circuiting ops
		if (op == RPN_LOGAND && !lhsVal) {
			setConstVal(lhs, 0);
			free(rhs);
			return lhs;
		}
		if (op == RPN_LOGOR && lhsVal) {
			setConstVal(lhs, 1);
			free(rhs);
			return lhs;
		}
		// Otherwise, if both are constant, we can compute the result unconditionally
		if (isConstant(rhs)) {
			struct Result res = rpn_ConstBinaryOp(lhsVal, op, getConstVal(rhs), NULL);

			free(rhs);
			if (is_ok(&res)) {
				setConstVal(lhs, res.value);
			} else if (res.error == ERR_UNK) {
				// This happens when we get a warning
				assert(lhs->rpnLength == 5); // Since LHS is supposed to be a constant...
				lhs = rpn_Grow(lhs, 5 + 1); // One extra constant plus operator
				memcpy(&lhs->rpn[5], rhs->rpn, 5);
				lhs->rpn[10] = op;
			} else {
				lhs->rpnLength = 1;
				lhs->rpn[0] = res.error;
			}
			free(rhs);
			return lhs;
		}
	}

	// FIXME: there's a possible overflow here
	lhs = rpn_Grow(lhs, rhs->rpnLength + 1);
	memcpy(&lhs->rpn[lhs->rpnLength - rhs->rpnLength - 1], rhs->rpn, rhs->rpnLength);
	lhs->rpn[lhs->rpnLength - 1] = op;
	free(rhs);
	return lhs;
}

/// EXPRESSION REDUCER

static void reportUnaryError(enum UnaryError type, int32_t value)
{
	switch (type) {
	case UN_ERR_HRAM:
		error("Value $%" PRIx32 " not in range [$FF00; $FFFF]\n", value);
		break;

	case UN_ERR_RST:
		error("Invalid address $%" PRIx32 " for RST\n", value);
		break;
	}
}

static void reportBinaryError(enum BinaryWarning type, int32_t lhs, int32_t rhs)
{
	switch (type) {
        case BIN_WARN_SHL_NEG:
        	warning(WARNING_SHIFT_AMOUNT, "Shifting left by negative amount %" PRId32 "\n",
        		rhs);
        	break;

        case BIN_WARN_SHL_LARGE:
        	warning(WARNING_SHIFT_AMOUNT, "Shfting left by large amount %" PRId32 "\n", rhs);
        	break;

        case BIN_WARN_NEG_SHR:
        	warning(WARNING_SHIFT, "Shifting right negative value %" PRId32 "\n", lhs);
        	break;

        case BIN_WARN_SHR_NEG:
        	warning(WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32 "\n",
        		rhs);
        	break;

        case BIN_WARN_SHR_LARGE:
        	warning(WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32 "\n", rhs);
        	break;

        case BIN_WARN_DIV:
        	assert(lhs == INT32_MIN);
        	assert(rhs == -1);
		warning(WARNING_DIV, "Division of %" PRId32 " by -1 yields %" PRId32 "\n",
			INT32_MIN, INT32_MIN);
		break;
	}
}

static size_t growRPNBuf(struct RPNBuffer *rpnBufPtr[MIN_NB_ELMS(1)], size_t capacity, size_t size)
{
	if (!*rpnBufPtr) {
		*rpnBufPtr = malloc(sizeof(**rpnBufPtr) * capacity);
		if (!*rpnBufPtr)
			fatalerror("Failed to grow RPN buffer: %s\n", strerror(errno));
		(*rpnBufPtr)->size = size;
		return capacity;
	}

	// FIXME: there's potential for overflow here
	(*rpnBufPtr)->size += size;
	if ((*rpnBufPtr)->size > capacity) {
		capacity *= 2;
		*rpnBufPtr = realloc(*rpnBufPtr, sizeof(**rpnBufPtr) * capacity);
		if (!rpnBufPtr)
			fatalerror("Failed to grow RPN buffer: %s\n", strerror(errno));
	}
	return capacity;
}

// Call this when discarding a potentially-unknown operand, so that its RPN expression gets purged
// if that's the case
static void discardOperand(struct RPNBuffer **rpnBufPtr, struct Result const *operand)
{
	// If the rhs is unknown, purge it from the RPN buf
	if (operand->error == ERR_UNK) {
		assert(rpnBufPtr);
		assert(*rpnBufPtr);
		assert((*rpnBufPtr)->size >= operand->exprSize);
		(*rpnBufPtr)->size -= operand->exprSize;
	}
}

// Le plat de résistance.
// Note: this assumes that the input is valid, and does not perform any bounds checking, except
// some assertions in debug mode.
// This is because the function is only used on internally-generated expressions, which should be
// okay anyway.
uint32_t rpn_Eval(struct Expression const *expr, struct RPNBuffer **rpnBufPtr)
{
	size_t i = 0; // Expr RPN buffer index
	size_t rpnBufCapacity = 256; // RPN buffer capacity

#define APPEND_BYTE(byte) do { \
	if (rpnBufPtr) { \
		rpnBufCapacity = growRPNBuf(rpnBufPtr, rpnBufCapacity, 1); \
		(*rpnBufPtr)->buf[(*rpnBufPtr)->size - 1] = (byte); \
	} \
} while (0)

// If the operand is an error, it must be propagated, not written to the buffer
// If the operand is unknown, it has already been writted to the buffer
#define APPEND_OPERAND(operand) do { \
	if (rpnBufPtr) { \
		if (is_ok((operand))) { \
			rpnBufCapacity = growRPNBuf(rpnBufPtr, rpnBufCapacity, 1 + sizeof(uint32_t)); \
			(*rpnBufPtr)->buf[0] = RPN_CONST; \
			writeLE32(&(*rpnBufPtr)->buf[1], (operand)->value); \
		} else if ((operand)->error == ERR_SYM) { \
			rpnBufCapacity = growRPNBuf(rpnBufPtr, rpnBufCapacity, 1 + sizeof(uint32_t)); \
			(*rpnBufPtr)->buf[0] = RPN_SYM; \
			writeLE32(&(*rpnBufPtr)->buf[1], out_GetSymbolID((operand)->symbol)); \
		} else if (is_err((operand))) { \
			APPEND_BYTE((operand)->error); \
		} \
	} \
} while(0)

#define DISCARD(operand) do { \
	if (rpnBufPtr) \
		discardOperand(rpnBufPtr, (operand)); \
} while (0)

	// RPN stack
	size_t capacity = 32; // 256 / sizeof(rpnStack[0]), somewhat arbitrarily
	size_t size = 0;
	struct Result *rpnStack = malloc(capacity);

// FIXME: capacity doubling may overflow
#define PUSH(entry) do { \
	if (size == capacity) { \
		capacity *= 2; \
		rpnStack = realloc(rpnStack, sizeof(rpnStack[0]) * capacity); \
		if (!rpnStack) \
			fatalerror("Failed to grow RPN stack: %s\n", strerror(errno)); \
	} \
	rpnStack[size++] = (entry); \
} while (0)

// !!! Be careful that popped results are invalidated as soon as you PUSH!
#define POP() (assert(size != 0), &rpnStack[--size])

	if (!rpnStack)
		fatalerror("Failed to alloc RPN stack: %s\n", strerror(errno));

	/// Now, process the input RPN

	while (i != expr->rpnLength) {
		assert(i < expr->rpnLength);

		enum RPNCommand opcode = expr->rpn[i++];

		switch (opcode) {
			struct Result *lhs, *rhs;
			char const *name;
			size_t nameLen;
			struct Section const *sect;
			struct Symbol *sym;
			uint32_t val;

		// Terminals

		case RPN_CONST:
			val = readLE32(&expr->rpn[i]);

			i += sizeof(val);
			PUSH(Ok(val));
			break;

		case RPN_SYM:
			// I'm sure this is fine...
			name = (char const *)&expr->rpn[i];
			// Check that there is a terminator somewhere
			// (The RPN is supposed to be valid, so elide this from release mode)
			assert(strnlen(name, expr->rpnLength - i) < expr->rpnLength - i);
			i += strlen(name);

			sym = sym_Ref(name);
			if (!sym_IsConstant(sym)) {
				PUSH(Sym(sym));
			} else {
				PUSH(Ok(sym_GetConstantSymValue(sym)));
			}
			break;

		case RPN_BANK_SELF:
			if (!currentSection) {
				PUSH(Err(ERR_NO_SELF_BANK));
			} else if (currentSection->bank == (uint32_t)-1) {
				APPEND_BYTE(RPN_BANK_SELF);
				PUSH(Unk(1));
			} else {
				PUSH(Ok(currentSection->bank));
			}
			break;

		case RPN_BANK_SYM:
			// I'm sure this is fine...
			name = (char const *)&expr->rpn[i];
			// Check that there is a terminator somewhere
			// (The RPN is supposed to be valid, so elide this from release mode)
			assert(strnlen(name, expr->rpnLength - i) < expr->rpnLength - i);
			nameLen = strlen(name);
			i += nameLen;

			sym = sym_Ref(name);
			assert(sym); // The above should have created the symbol if it didn't exist
			if (!sym_IsLabel(sym)) {
				PUSH(Err(ERR_BANK_NOT_SYM));
			} else if (sym_GetSection(sym)->bank != (uint32_t)-1) {
				PUSH(Ok(sym_GetSection(sym)->bank));
			} else {
				size_t len = 1 + nameLen + 1; // RPN_BANK_SYM, name, terminator

				if (rpnBufPtr) {
					rpnBufCapacity = growRPNBuf(rpnBufPtr, rpnBufCapacity, len);
					uint8_t *ptr = &(*rpnBufPtr)->buf[(*rpnBufPtr)->size - len];

					*ptr++ = RPN_BANK_SYM;
					memcpy(ptr, name, nameLen + 1);
				}
				PUSH(Unk(len));
			}
			break;

		case RPN_BANK_SECT:
			// I'm sure this is fine...
			name = (char const *)&expr->rpn[i];
			// Check that there is a terminator somewhere
			// (The RPN is supposed to be valid, so elide this from release mode)
			assert(strnlen(name, expr->rpnLength - i) < expr->rpnLength - i);
			nameLen = strlen(name);
			i += nameLen;

			sect = out_FindSectionByName(name);
			if (sect && sect->bank != (uint32_t)-1) {
				PUSH(Ok(sect->bank));
			} else {
				size_t len = 1 + nameLen + 1; // RPN_BANK_SECT, name, terminator

				if (rpnBufPtr) {
					rpnBufCapacity = growRPNBuf(rpnBufPtr, rpnBufCapacity, len);
					uint8_t *ptr = &(*rpnBufPtr)->buf[(*rpnBufPtr)->size - len];

					*ptr++ = RPN_BANK_SYM;
					memcpy(ptr, name, nameLen + 1);
				}
				PUSH(Unk(len));
			}
			break;

		case RPN_SIZEOF_SECT:
		case RPN_STARTOF_SECT:
			// Due to SECTION FRAGMENT, neither of those can be computed by RGBASM
			// TODO: actually, this is not entirely the case: the size of a "basic" one
			// is known if it's not the current section and isn't on the stack, and its
			// address may be known, too. (STARTOF of a fixed SECTION UNION is also
			// constant.)
			// I'm sure this is fine...
			name = (char const *)&expr->rpn[i];
			// Check that there is a terminator somewhere
			// (The RPN is supposed to be valid, so elide this from release mode)
			assert(strnlen(name, expr->rpnLength - i) < expr->rpnLength - i);
			nameLen = strlen(name);
			i += nameLen;
			size_t len = 1 + nameLen + 1;

			if (rpnBufPtr) {
				rpnBufCapacity = growRPNBuf(rpnBufPtr, rpnBufCapacity, len);
				uint8_t *ptr = &(*rpnBufPtr)->buf[(*rpnBufPtr)->size - len];

				*ptr++ = opcode;
				memcpy(ptr, name, nameLen + 1);
			}
			PUSH(Unk(len));
			break;

		// Unary operators

		case RPN_ISCONST:
			lhs = POP();
			DISCARD(lhs);
			PUSH(Ok(is_ok(lhs)));
			break;

		case RPN_UNSUB:
		case RPN_UNNOT:
		case RPN_LOGUNNOT:
		case RPN_HRAM:
		case RPN_RST:
			lhs = POP();
			if (is_ok(lhs)) {
				lhs->value = rpn_ConstUnaryOp(opcode, lhs->value,
							      reportUnaryError);
			} else if (is_unk(lhs)) {
				APPEND_BYTE(opcode);
			} // Preserve errors
			size++; // Push the modified entry back
			break;

		// Binary operators

		case RPN_ADD:
		case RPN_SUB: // May be constant for same-section symbol subtraction
		case RPN_MUL:
		case RPN_DIV:
		case RPN_MOD:
		case RPN_EXP:
		case RPN_OR:
		case RPN_AND:
		case RPN_XOR:
		case RPN_LOGAND: // May be short-circuiting
		case RPN_LOGOR: // May be short-circuiting
		case RPN_LOGEQ:
		case RPN_LOGNE:
		case RPN_LOGGT:
		case RPN_LOGLT:
		case RPN_LOGGE:
		case RPN_LOGLE:
		case RPN_SHL:
		case RPN_SHR:
			rhs = POP();
			lhs = POP();

			if (is_ok(lhs) && is_ok(rhs)) {
				PUSH(rpn_ConstBinaryOp(lhs->value, opcode, rhs->value,
						       reportBinaryError));
				break;
			}

			// Logical AND/OR are short-circuiting
			if (is_ok(lhs)) {
				if (opcode == RPN_LOGAND) {
					if (lhs->value == 0) {
						DISCARD(rhs);
						PUSH(Ok(0));
						break;
					}
				} else if (opcode == RPN_LOGOR) {
					if (lhs->value != 0) {
						DISCARD(rhs);
						PUSH(Ok(1));
						break;
					}
				}
			}

			// If either operand is an error, return that error
			// However, short-circuiting may end up discarding some errors at link time;
			// in those cases, write the error to the RPN buffer instead
			if (is_err(lhs)) {
				DISCARD(rhs);
				// Instead of copying `lhs` over itself, just increment the size
				size++;
				break;
			}
			if (is_err(rhs) && opcode != RPN_LOGAND && opcode != RPN_LOGOR) {
				DISCARD(lhs);
				PUSH(*rhs);
				break;
			}

			// Subtracting two symbols in the same section is still constant
			if (opcode == RPN_SUB) {
			        if (lhs->error == ERR_SYM && rhs->error == ERR_SYM
			         && lhs->symbol->section == rhs->symbol->section) {
				        // Non-numeric symbols are rejected while parsing
				        assert(sym_IsNumeric(lhs->symbol));
				        assert(sym_IsNumeric(rhs->symbol));
					// Subtract their offset within the section
					PUSH(Ok(lhs->symbol->value - rhs->symbol->value));
					break;
				}
			}

			APPEND_OPERAND(lhs);
			APPEND_OPERAND(rhs);
			APPEND_BYTE(opcode);
			size_t exprSize = 1; // Size of the corresponding RPN expression

			// FIXME: potential overflows here
			if (is_unk(lhs))
				exprSize += lhs->exprSize;
			if (is_unk(rhs))
				exprSize += rhs->exprSize;
			PUSH(Unk(exprSize));
			break;

		// Errors
		case RPN_ERR_NO_SELF_BANK:
		case RPN_ERR_DIV_BY_0:
		case RPN_ERR_MOD_BY_0:
		case RPN_ERR_BANK_NOT_SYM:
		case RPN_ERR_EXP_NEG_POW:
			PUSH(Err(opcode));
			break;
		}
	}

#undef PUSH
#undef POP
#undef DISCARD

	assert(size == 1);

	if (is_ok(&rpnStack[0])) {
		if (rpnBufPtr && *rpnBufPtr) {
			free(*rpnBufPtr);
			*rpnBufPtr = NULL;
		}
		return rpnStack[0].value;
	}

	if (is_err(&rpnStack[0])) {
		// Yield the error
		// TODO
		if (rpnBufPtr) {
			free(*rpnBufPtr);
			*rpnBufPtr = NULL;
		}
		return 0;
	}

	// If the value isn't constant, we must have somewhere to write the expression to
	if (!rpnBufPtr) {
		// TODO
		error("Expected constant expression: <REASON>\n");
	}

	// If the value is unknown, then the whole expression is already on the stack
	// But it might also be a symbol, in which case it needs to be written
	if (rpnStack[0].error == ERR_SYM)
		APPEND_OPERAND(&rpnStack[0]);

#undef APPEND_BYTE
#undef APPEND_OPERAND

	return 0;
}

void rpn_CheckNBit(struct Expression const *expr, uint8_t n)
{
	assert(n != 0); // That doesn't make sense
	assert(n <= CHAR_BIT * sizeof(int)); // Otherwise `1u << (n - 1)` is UB

	if (isConstant(expr)) {
		uint32_t val = getConstVal(expr);

		if (val < -(1u << (n - 1)) || val >= 1u << (n - 1))
			warning(WARNING_TRUNCATION, "Expression must be %u-bit\n", n);
	}
}
