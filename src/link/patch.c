/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "link/object.h"
#include "link/patch.h"
#include "link/section.h"
#include "link/symbol.h"

#include "linkdefs.h"
#include "opmath.h"

#include "extern/err.h"

enum ErrorType {
	NO_ERR,
#define ERR(name) ERR_##name = RPN_ERR_##name
	ERR(NO_SELF_BANK), // BANK(@) outside of a SECTION
	ERR(DIV_BY_0),     // Division by 0
	ERR(BANK_NOT_SYM), // BANK(Sym), but `Sym` is not a label
#undef ERR
};

/*
 * This is an "empty"-type stack. Apart from the actual values, we also remember
 * whether the value is a placeholder inserted for error recovery. This allows
 * us to avoid cascading errors.
 *
 * The best way to think about this is a stack of (value, errorFlag) pairs.
 * They are only separated for reasons of memory efficiency.
 */
struct RPNStack {
	struct Entry {
		int32_t val;
		enum ErrorType errType;
	} *entries;
	size_t size;
	size_t capacity;
} stack;

static void initRPNStack(void)
{
	stack.capacity = 64;
	stack.entries = malloc(sizeof(*stack.entries) * stack.capacity);
	if (!stack.entries)
		fatal(NULL, 0, "Failed to init RPN stack: %s", strerror(errno));
}

static void clearRPNStack(void)
{
	stack.size = 0;
}

static void pushRPN(int32_t value, enum ErrorType errType)
{
	if (stack.size >= stack.capacity) {
		static const size_t increase_factor = 2;

		if (stack.capacity > SIZE_MAX / increase_factor)
			fatal(NULL, 0, "Overflow in RPN stack resize");

		stack.capacity *= increase_factor;
		stack.entries = realloc(stack.entries, sizeof(*stack.entries) * stack.capacity);
		/*
		 * Static analysis tools complain that the capacity might become
		 * zero due to overflow, but fail to realize that it's caught by
		 * the overflow check above. Hence the stringent check below.
		 */
		if (!stack.entries || !stack.capacity)
			fatal(NULL, 0, "Failed to resize RPN stack: %s", strerror(errno));
	}

	stack.entries[stack.size] = (struct Entry){ .val = value, .errType = errType };
	stack.size++;
}

static struct Entry *popRPN(struct FileStackNode const *node, uint32_t lineNo)
{
	if (stack.size == 0)
		fatal(node, lineNo, "Internal error, RPN stack empty");

	stack.size--;
	return &stack.entries[stack.size];
}

static void freeRPNStack(void)
{
	free(stack.entries);
}

/* RPN operators */

static uint32_t getRPNByte(uint8_t const **expression, int32_t *size,
			   struct FileStackNode const *node, uint32_t lineNo)
{
	if (!(*size)--)
		fatal(node, lineNo, "Internal error, RPN expression overread");

	return *(*expression)++;
}

static struct Symbol const *getSymbol(struct Symbol const * const *symbolList, uint32_t index)
{
	assert(index != -1); /* PC needs to be handled specially, not here */
	struct Symbol const *symbol = symbolList[index];

	/* If the symbol is defined elsewhere... */
	if (symbol->type == SYMTYPE_IMPORT)
		return sym_GetSymbol(symbol->name);

	return symbol;
}

/**
 * Compute a patch's value from its RPN string.
 * @param patch The patch to compute the value of
 * @param section The section the patch is contained in
 * @return The patch's value
 * @return isError Set if an error occurred during evaluation, and further
 *                 errors caused by the value should be suppressed.
 */
static int32_t computeRPNExpr(struct Patch const *patch,
			      struct Symbol const * const *fileSymbols)
{
/* Small shortcut to avoid a lot of repetition */
#define popRPN() popRPN(patch->src, patch->lineNo)

	uint8_t const *expression = patch->rpnExpression;
	int32_t size = patch->rpnSize;

	clearRPNStack();

	while (size > 0) {
		enum RPNCommand command = getRPNByte(&expression, &size,
						     patch->src, patch->lineNo);
		enum ErrorType errType = NO_ERR;
		int32_t value;

		switch (command) {
			struct Symbol const *symbol;
			char const *name;
			struct Section const *sect;
			struct Entry *lhs, *rhs;

		case RPN_BANK_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(&expression, &size,
						    patch->src, patch->lineNo) << shift;
			symbol = getSymbol(fileSymbols, value);

			if (!symbol) {
				error(patch->src, patch->lineNo,
				      "Requested BANK() of symbol \"%s\", which was not found",
				      fileSymbols[value]->name);
				isError = true;
				value = 1;
			} else if (!symbol->section) {
				error(patch->src, patch->lineNo,
				      "Requested BANK() of non-label symbol \"%s\"",
				      fileSymbols[value]->name);
				isError = true;
				value = 1;
			} else {
				value = symbol->section->bank;
			}
			break;

		case RPN_BANK_SECT:
			/*
			 * `expression` is not guaranteed to be '\0'-terminated. If it is not,
			 * `getRPNByte` will have a fatal internal error.
			 * In either case, `getRPNByte` will not free `expression`.
			 */
			name = (char const *)expression;
			while (getRPNByte(&expression, &size, patch->src, patch->lineNo))
				;

			sect = sect_GetSection(name);

			if (!sect) {
				error(patch->src, patch->lineNo,
				      "Requested BANK() of section \"%s\", which was not found",
				      name);
				isError = true;
				value = 1;
			} else {
				value = sect->bank;
			}
			break;

		case RPN_BANK_SELF:
			if (!patch->pcSection) {
				error(patch->src, patch->lineNo,
				      "PC has no bank outside a section");
				isError = true;
				value = 1;
			} else {
				value = patch->pcSection->bank;
			}
			break;

		case RPN_SIZEOF_SECT:
			/* This has assumptions commented in the `RPN_BANK_SECT` case above. */
			name = (char const *)expression;
			while (getRPNByte(&expression, &size, patch->src, patch->lineNo))
				;

			sect = sect_GetSection(name);

			if (!sect) {
				error(patch->src, patch->lineNo,
				      "Requested SIZEOF() of section \"%s\", which was not found",
				      name);
				isError = true;
				value = 1;
			} else {
				value = sect->size;
			}
			break;

		case RPN_STARTOF_SECT:
			/* This has assumptions commented in the `RPN_BANK_SECT` case above. */
			name = (char const *)expression;
			while (getRPNByte(&expression, &size, patch->src, patch->lineNo))
				;

			sect = sect_GetSection(name);

			if (!sect) {
				error(patch->src, patch->lineNo,
				      "Requested STARTOF() of section \"%s\", which was not found",
				      name);
				isError = true;
				value = 1;
			} else {
				value = sect->org;
			}
			break;

		case RPN_HRAM:
			value = popRPN();
			if (!isError && (value < 0
				     || (value > 0xFF && value < 0xFF00)
				     || value > 0xFFFF)) {
				error(patch->src, patch->lineNo,
				      "Value %" PRId32 " is not in HRAM range", value);
				isError = true;
			}
			value &= 0xFF;
			break;

		case RPN_RST:
			value = popRPN();
			/* Acceptable values are 0x00, 0x08, 0x10, ..., 0x38
			 * They can be easily checked with a bitmask
			 */
			if (value & ~0x38) {
				if (!isError)
					error(patch->src, patch->lineNo,
					      "Value %" PRId32 " is not a RST vector", value);
				isError = true;
			}
			value |= 0xC7;
			break;

		case RPN_CONST:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(&expression, &size,
						    patch->src, patch->lineNo) << shift;
			break;

		case RPN_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(&expression, &size,
						    patch->src, patch->lineNo) << shift;

			if (value == -1) { /* PC */
				if (!patch->pcSection) {
					error(patch->src, patch->lineNo,
					      "PC has no value outside a section");
					value = 0;
					isError = true;
				} else {
					value = patch->pcOffset + patch->pcSection->org;
				}
			} else {
				symbol = getSymbol(fileSymbols, value);

				if (!symbol) {
					error(patch->src, patch->lineNo,
					      "Unknown symbol \"%s\"", fileSymbols[value]->name);
					isError = true;
				} else {
					value = symbol->value;
					/* Symbols attached to sections have offsets */
					if (symbol->section)
						value += symbol->section->org;
				}
			}
			break;

		case RPN_ADD:
		case RPN_SUB:
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
			rhs = popRPN();
			lhs = popRPN();

			// Propagate the LHS' error, if any
			if (lhs->errType != NO_ERR) {
				errType = lhs->errType;
				break;
			}
			// Attempt short-circuiting
			if (command == RPN_LOGAND && lhs->value == 0) {
				value = 0;
				break;
			} else if (command == RPN_LOGOR && lhs->value != 0) {
				value = 1;
				break;
			}
			// Propagare the RHS' error, if any
			if (rhs->errType != NO_ERR) {
				errType = rhs->errType;
				break;
			}

			struct Result res = rpn_ConstBinaryOp(lhs->val, command, rhs->val);

			// TODO
			break;

		case RPN_ERR_NO_SELF_BANK:
		case RPN_ERR_DIV_BY_0:
		case RPN_ERR_MOD_BY_0:
		case RPN_ERR_BANK_NOT_SYM:
		case RPN_ERR_EXP_NEG_POW:
			// TODO
			break;

		case RPN_ISCONST:
			error("Bad object file: RPN_ISCONST is not valid in object files");
			break;
		}

		pushRPN(value, isError);
	}

	if (stack.size > 1)
		error(patch->src, patch->lineNo,
		      "RPN stack has %zu entries on exit, not 1", stack.size);

	isError = false;
	return popRPN();

#undef popRPN
}

void patch_CheckAssertions(struct Assertion *assert)
{
	verbosePrint("Checking assertions...\n");
	initRPNStack();

	while (assert) {
		int32_t value = computeRPNExpr(&assert->patch,
			(struct Symbol const * const *)assert->fileSymbols);
		enum AssertionType type = (enum AssertionType)assert->patch.type;

		if (!isError && !value) {
			switch (type) {
			case ASSERT_FATAL:
				fatal(assert->patch.src, assert->patch.lineNo, "%s",
				      assert->message[0] ? assert->message
							 : "assert failure");
				/* Not reached */
				break; /* Here so checkpatch doesn't complain */
			case ASSERT_ERROR:
				error(assert->patch.src, assert->patch.lineNo, "%s",
				      assert->message[0] ? assert->message
							 : "assert failure");
				break;
			case ASSERT_WARN:
				warning(assert->patch.src, assert->patch.lineNo, "%s",
					assert->message[0] ? assert->message
							   : "assert failure");
				break;
			}
		} else if (isError && type == ASSERT_FATAL) {
			fatal(assert->patch.src, assert->patch.lineNo,
			      "couldn't evaluate assertion%s%s",
			      assert->message[0] ? ": " : "",
			      assert->message);
		}
		struct Assertion *next = assert->next;

		free(assert);
		assert = next;
	}

	freeRPNStack();
}

/**
 * Applies all of a section's patches
 * @param section The section to patch
 * @param arg Ignored callback arg
 */
static void applyFilePatches(struct Section *section, struct Section *dataSection)
{
	if (!sect_HasData(section->type))
		return;

	verbosePrint("Patching section \"%s\"...\n", section->name);
	for (uint32_t patchID = 0; patchID < section->nbPatches; patchID++) {
		struct Patch *patch = &section->patches[patchID];
		int32_t value = computeRPNExpr(patch,
					       (struct Symbol const * const *)
							section->fileSymbols);
		uint16_t offset = patch->offset + section->offset;

		/* `jr` is quite unlike the others... */
		if (patch->type == PATCHTYPE_JR) {
			// Offset is relative to the byte *after* the operand
			// PC as operand to `jr` is lower than reference PC by 2
			uint16_t address = patch->pcSection->org
							+ patch->pcOffset + 2;
			int16_t jumpOffset = value - address;

			if (!isError && (jumpOffset < -128 || jumpOffset > 127))
				error(patch->src, patch->lineNo,
				      "jr target out of reach (expected -129 < %" PRId16 " < 128)",
				      jumpOffset);
			dataSection->data[offset] = jumpOffset & 0xFF;
		} else {
			/* Patch a certain number of bytes */
			struct {
				uint8_t size;
				int32_t min;
				int32_t max;
			} const types[] = {
				[PATCHTYPE_BYTE] = {1,      -128,       255},
				[PATCHTYPE_WORD] = {2,    -32768,     65536},
				[PATCHTYPE_LONG] = {4, INT32_MIN, INT32_MAX}
			};

			if (!isError && (value < types[patch->type].min
				      || value > types[patch->type].max))
				error(patch->src, patch->lineNo,
				      "Value %#" PRIx32 "%s is not %u-bit",
				      value, value < 0 ? " (maybe negative?)" : "",
				      types[patch->type].size * 8U);
			for (uint8_t i = 0; i < types[patch->type].size; i++) {
				dataSection->data[offset + i] = value & 0xFF;
				value >>= 8;
			}
		}
	}
}

/**
 * Applies all of a section's patches, iterating over "components" of
 * unionized sections
 * @param section The section to patch
 * @param arg Ignored callback arg
 */
static void applyPatches(struct Section *section, void *arg)
{
	(void)arg;
	struct Section *dataSection = section;

	do {
		applyFilePatches(section, dataSection);
		section = section->nextu;
	} while (section);
}

void patch_ApplyPatches(void)
{
	initRPNStack();
	sect_ForEach(applyPatches, NULL);
	freeRPNStack();
}

