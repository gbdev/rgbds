
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "link/patch.h"
#include "link/section.h"
#include "link/symbol.h"

#include "linkdefs.h"

#include "extern/err.h"

/* This is an "empty"-type stack */
struct RPNStack {
	int32_t *buf;
	size_t size;
	size_t capacity;
} stack;

static inline void initRPNStack(void)
{
	stack.capacity = 64;
	stack.buf = malloc(sizeof(*stack.buf) * stack.capacity);
	if (!stack.buf)
		err(1, "Failed to init RPN stack");
}

static inline void clearRPNStack(void)
{
	stack.size = 0;
}

static void pushRPN(int32_t value)
{
	if (stack.size >= stack.capacity) {
		stack.capacity *= 2;
		stack.buf =
			realloc(stack.buf, sizeof(*stack.buf) * stack.capacity);
		if (!stack.buf)
			err(1, "Failed to resize RPN stack");
	}

	stack.buf[stack.size] = value;
	stack.size++;
}

static int32_t popRPN(void)
{
	if (stack.size == 0)
		errx(1, "Internal error, RPN stack empty");

	stack.size--;
	return stack.buf[stack.size];
}

static inline void freeRPNStack(void)
{
	free(stack.buf);
}

/* RPN operators */

static uint8_t getRPNByte(uint8_t const **expression, int32_t *size,
			  char const *fileName, int32_t lineNo)
{
	if (!(*size)--)
		errx(1, "%s(%d): RPN expression overread", fileName, lineNo);
	return *(*expression)++;
}

/**
 * Compute a patch's value from its RPN string.
 * @param patch The patch to compute the value of
 * @param section The section the patch is contained in
 * @return The patch's value
 */
static int32_t computeRPNExpr(struct Patch const *patch,
			      struct Section const *section)
{
	uint8_t const *expression = patch->rpnExpression;
	int32_t size = patch->rpnSize;

	clearRPNStack();

	while (size > 0) {
		enum RPNCommand command = getRPNByte(&expression, &size,
						     patch->fileName,
						     patch->lineNo);
		int32_t value;

		/*
		 * Friendly reminder:
		 * Be VERY careful with two `popRPN` in the same expression.
		 * C does not guarantee order of evaluation of operands!!
		 * So, if there are two `popRPN` in the same expression, make
		 * sure the operation is commutative.
		 */
		switch (command) {
			struct Symbol const *symbol;
			char const *name;
			struct Section const *sect;

		case RPN_ADD:
			value = popRPN() + popRPN();
			break;
		case RPN_SUB:
			value = popRPN();
			value = popRPN() - value;
			break;
		case RPN_MUL:
			value = popRPN() * popRPN();
			break;
		case RPN_DIV:
			value = popRPN();
			value = popRPN() / value;
			break;
		case RPN_MOD:
			value = popRPN();
			value = popRPN() % value;
			break;
		case RPN_UNSUB:
			value = -popRPN();
			break;

		case RPN_OR:
			value = popRPN() | popRPN();
			break;
		case RPN_AND:
			value = popRPN() & popRPN();
			break;
		case RPN_XOR:
			value = popRPN() ^ popRPN();
			break;
		case RPN_UNNOT:
			value = ~popRPN();
			break;

		case RPN_LOGAND:
			value = popRPN();
			value = popRPN() && value;
			break;
		case RPN_LOGOR:
			value = popRPN();
			value = popRPN() || value;
			break;
		case RPN_LOGUNNOT:
			value = !popRPN();
			break;

		case RPN_LOGEQ:
			value = popRPN() == popRPN();
			break;
		case RPN_LOGNE:
			value = popRPN() != popRPN();
			break;
		case RPN_LOGGT:
			value = popRPN();
			value = popRPN() > value;
			break;
		case RPN_LOGLT:
			value = popRPN();
			value = popRPN() < value;
			break;
		case RPN_LOGGE:
			value = popRPN();
			value = popRPN() >= value;
			break;
		case RPN_LOGLE:
			value = popRPN();
			value = popRPN() <= value;
			break;

		/* FIXME: sanitize shifts */
		case RPN_SHL:
			value = popRPN();
			value = popRPN() << value;
			break;
		case RPN_SHR:
			value = popRPN();
			value = popRPN() >> value;
			break;

		case RPN_BANK_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(&expression, &size,
						    patch->fileName,
						    patch->lineNo) << shift;

			symbol = section->fileSymbols[value];

			/* If the symbol is defined elsewhere... */
			if (symbol->type == SYMTYPE_IMPORT) {
				symbol = sym_GetSymbol(symbol->name);
				if (!symbol)
					errx(1, "%s(%d): Unknown symbol \"%s\"",
					     patch->fileName, patch->lineNo,
					     symbol->name);
			}

			value = symbol->section->bank;
			break;

		case RPN_BANK_SECT:
			name = (char const *)expression;
			while (getRPNByte(&expression, &size, patch->fileName,
					  patch->lineNo))
				;

			sect = sect_GetSection(name);

			if (!sect)
				errx(1, "%s(%d): Requested BANK() of section \"%s\", which was not found",
				     patch->fileName, patch->lineNo, name);

			value = sect->bank;
			break;

		case RPN_BANK_SELF:
			value = section->bank;
			break;

		case RPN_HRAM:
			value = popRPN();
			if (value < 0
			 || (value > 0xFF && value < 0xFF00)
			 || value > 0xFFFF)
				errx(1, "%s(%d): Value %d is not in HRAM range",
				     patch->fileName, patch->lineNo, value);
			value &= 0xFF;
			break;

		case RPN_CONST:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(&expression, &size,
						    patch->fileName,
						    patch->lineNo) << shift;
			break;

		case RPN_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(&expression, &size,
						    patch->fileName,
						    patch->lineNo) << shift;

			symbol = section->fileSymbols[value];

			/* If the symbol is defined elsewhere... */
			if (symbol->type == SYMTYPE_IMPORT) {
				symbol = sym_GetSymbol(symbol->name);
				if (!symbol)
					errx(1, "%s(%d): Unknown symbol \"%s\"",
					     patch->fileName, patch->lineNo,
					     symbol->name);
			}

			if (!strcmp(symbol->name, "@")) {
				value = section->org + patch->offset;
			} else {
				value = symbol->value;
				/* Symbols attached to sections have offsets */
				if (symbol->section)
					value += symbol->section->org;
			}
			break;
		}

		pushRPN(value);
	}

	if (stack.size > 1)
		warnx("%s(%d): RPN stack has %lu entries on exit, not 1",
		      patch->fileName, patch->lineNo, stack.size);

	return popRPN();
}

/**
 * Applies all of a section's patches
 * @param section The section to patch
 * @param arg Ignored callback arg
 */
static void applyPatches(struct Section *section, void *arg)
{
	(void)arg;

	if (!sect_HasData(section->type))
		return;

	verbosePrint("Patching section \"%s\"...\n", section->name);
	for (uint32_t patchID = 0; patchID < section->nbPatches; patchID++) {
		struct Patch *patch = &section->patches[patchID];
		int32_t value = computeRPNExpr(patch, section);

		if (patch->type == PATCHTYPE_JR) {
			/* `jr` is quite unlike the others... */
			uint16_t address = section->org + patch->offset;
			/* Target is relative to the byte *after* the operand */
			int32_t offset = value - (address + 1);

			if (offset < -128 || offset > 127)
				errx(1, "%s(%d): jr target out of reach (%d)",
				     patch->fileName, patch->lineNo, offset);
			section->data[patch->offset] = offset & 0xFF;
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

			if (value < types[patch->type].min
			 || value > types[patch->type].max)
				errx(1, "%s(%d): Value %#x%s is not %u-bit",
				     patch->fileName, patch->lineNo, value,
				     value < 0 ? " (maybe negative?)" : "",
				     types[patch->type].size * 8);
			for (uint8_t i = 0; i < types[patch->type].size; i++) {
				section->data[patch->offset + i] = value & 0xFF;
				value >>= 8;
			}
		}
	}
}

void patch_ApplyPatches(void)
{
	initRPNStack();
	sect_ForEach(applyPatches, NULL);
	freeRPNStack();
}
