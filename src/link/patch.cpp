/* SPDX-License-Identifier: MIT */

#include "link/patch.hpp"

#include <deque>
#include <inttypes.h>
#include <stdint.h>
#include <variant>
#include <vector>

#include "helpers.hpp" // assume, clz, ctz
#include "linkdefs.hpp"
#include "opmath.hpp"

#include "link/main.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"

std::deque<Assertion> assertions;

struct RPNStackEntry {
	int32_t value;
	bool errorFlag; // Whether the value is a placeholder inserted for error recovery
};

std::deque<RPNStackEntry> rpnStack;

static void pushRPN(int32_t value, bool comesFromError) {
	rpnStack.push_front({.value = value, .errorFlag = comesFromError});
}

// This flag tracks whether the RPN op that is currently being evaluated
// has popped any values with the error flag set.
static bool isError = false;

static int32_t popRPN(Patch const &patch) {
	if (rpnStack.empty())
		fatal(patch.src, patch.lineNo, "Internal error, RPN stack empty");

	RPNStackEntry entry = rpnStack.front();

	rpnStack.pop_front();
	isError |= entry.errorFlag;
	return entry.value;
}

// RPN operators

static uint32_t getRPNByte(uint8_t const *&expression, int32_t &size, Patch const &patch) {
	if (!size--)
		fatal(patch.src, patch.lineNo, "Internal error, RPN expression overread");

	return *expression++;
}

static Symbol const *getSymbol(std::vector<Symbol> const &symbolList, uint32_t index) {
	assume(index != (uint32_t)-1); // PC needs to be handled specially, not here
	Symbol const &symbol = symbolList[index];

	// If the symbol is defined elsewhere...
	if (symbol.type == SYMTYPE_IMPORT)
		return sym_GetSymbol(symbol.name);

	return &symbol;
}

/*
 * Compute a patch's value from its RPN string.
 * @param patch The patch to compute the value of
 * @param section The section the patch is contained in
 * @return The patch's value
 * @return isError Set if an error occurred during evaluation, and further
 *                 errors caused by the value should be suppressed.
 */
static int32_t computeRPNExpr(Patch const &patch, std::vector<Symbol> const &fileSymbols) {
	uint8_t const *expression = patch.rpnExpression.data();
	int32_t size = (int32_t)patch.rpnExpression.size();

	rpnStack.clear();

	while (size > 0) {
		RPNCommand command = (RPNCommand)getRPNByte(expression, size, patch);
		int32_t value;

		isError = false;

		// Be VERY careful with two `popRPN` in the same expression.
		// C++ does not guarantee order of evaluation of operands!
		// So, if there are two `popRPN` in the same expression, make
		// sure the operation is commutative.
		switch (command) {
		case RPN_ADD:
			value = popRPN(patch) + popRPN(patch);
			break;
		case RPN_SUB:
			value = popRPN(patch);
			value = popRPN(patch) - value;
			break;
		case RPN_MUL:
			value = popRPN(patch) * popRPN(patch);
			break;
		case RPN_DIV:
			value = popRPN(patch);
			if (value == 0) {
				if (!isError)
					error(patch.src, patch.lineNo, "Division by 0");
				isError = true;
				popRPN(patch);
				value = INT32_MAX;
			} else {
				value = op_divide(popRPN(patch), value);
			}
			break;
		case RPN_MOD:
			value = popRPN(patch);
			if (value == 0) {
				if (!isError)
					error(patch.src, patch.lineNo, "Modulo by 0");
				isError = true;
				popRPN(patch);
				value = 0;
			} else {
				value = op_modulo(popRPN(patch), value);
			}
			break;
		case RPN_NEG:
			value = -popRPN(patch);
			break;
		case RPN_EXP:
			value = popRPN(patch);
			if (value < 0) {
				if (!isError)
					error(patch.src, patch.lineNo, "Exponent by negative");
				isError = true;
				popRPN(patch);
				value = 0;
			} else {
				value = op_exponent(popRPN(patch), value);
			}
			break;

		case RPN_HIGH:
			value = (popRPN(patch) >> 8) & 0xFF;
			break;
		case RPN_LOW:
			value = popRPN(patch) & 0xFF;
			break;

		case RPN_BITWIDTH:
			value = popRPN(patch);
			value = value != 0 ? 32 - clz((uint32_t)value) : 0;
			break;
		case RPN_TZCOUNT:
			value = popRPN(patch);
			value = value != 0 ? ctz((uint32_t)value) : 32;
			break;

		case RPN_OR:
			value = popRPN(patch) | popRPN(patch);
			break;
		case RPN_AND:
			value = popRPN(patch) & popRPN(patch);
			break;
		case RPN_XOR:
			value = popRPN(patch) ^ popRPN(patch);
			break;
		case RPN_NOT:
			value = ~popRPN(patch);
			break;

		case RPN_LOGAND:
			value = popRPN(patch);
			value = popRPN(patch) && value;
			break;
		case RPN_LOGOR:
			value = popRPN(patch);
			value = popRPN(patch) || value;
			break;
		case RPN_LOGNOT:
			value = !popRPN(patch);
			break;

		case RPN_LOGEQ:
			value = popRPN(patch) == popRPN(patch);
			break;
		case RPN_LOGNE:
			value = popRPN(patch) != popRPN(patch);
			break;
		case RPN_LOGGT:
			value = popRPN(patch);
			value = popRPN(patch) > value;
			break;
		case RPN_LOGLT:
			value = popRPN(patch);
			value = popRPN(patch) < value;
			break;
		case RPN_LOGGE:
			value = popRPN(patch);
			value = popRPN(patch) >= value;
			break;
		case RPN_LOGLE:
			value = popRPN(patch);
			value = popRPN(patch) <= value;
			break;

		case RPN_SHL:
			value = popRPN(patch);
			value = op_shift_left(popRPN(patch), value);
			break;
		case RPN_SHR:
			value = popRPN(patch);
			value = op_shift_right(popRPN(patch), value);
			break;
		case RPN_USHR:
			value = popRPN(patch);
			value = op_shift_right_unsigned(popRPN(patch), value);
			break;

		case RPN_BANK_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(expression, size, patch) << shift;

			if (Symbol const *symbol = getSymbol(fileSymbols, value); !symbol) {
				error(
				    patch.src,
				    patch.lineNo,
				    "Requested BANK() of symbol \"%s\", which was not found",
				    fileSymbols[value].name.c_str()
				);
				isError = true;
				value = 1;
			} else if (auto *label = std::get_if<Label>(&symbol->data); label) {
				value = label->section->bank;
			} else {
				error(
				    patch.src,
				    patch.lineNo,
				    "Requested BANK() of non-label symbol \"%s\"",
				    fileSymbols[value].name.c_str()
				);
				isError = true;
				value = 1;
			}
			break;

		case RPN_BANK_SECT: {
			// `expression` is not guaranteed to be '\0'-terminated. If it is not,
			// `getRPNByte` will have a fatal internal error.
			char const *name = (char const *)expression;
			while (getRPNByte(expression, size, patch))
				;

			if (Section const *sect = sect_GetSection(name); !sect) {
				error(
				    patch.src,
				    patch.lineNo,
				    "Requested BANK() of section \"%s\", which was not found",
				    name
				);
				isError = true;
				value = 1;
			} else {
				value = sect->bank;
			}
			break;
		}

		case RPN_BANK_SELF:
			if (!patch.pcSection) {
				error(patch.src, patch.lineNo, "PC has no bank outside a section");
				isError = true;
				value = 1;
			} else {
				value = patch.pcSection->bank;
			}
			break;

		case RPN_SIZEOF_SECT: {
			// This has assumptions commented in the `RPN_BANK_SECT` case above.
			char const *name = (char const *)expression;
			while (getRPNByte(expression, size, patch))
				;

			if (Section const *sect = sect_GetSection(name); !sect) {
				error(
				    patch.src,
				    patch.lineNo,
				    "Requested SIZEOF() of section \"%s\", which was not found",
				    name
				);
				isError = true;
				value = 1;
			} else {
				value = sect->size;
			}
			break;
		}

		case RPN_STARTOF_SECT: {
			// This has assumptions commented in the `RPN_BANK_SECT` case above.
			char const *name = (char const *)expression;
			while (getRPNByte(expression, size, patch))
				;

			if (Section const *sect = sect_GetSection(name); !sect) {
				error(
				    patch.src,
				    patch.lineNo,
				    "Requested STARTOF() of section \"%s\", which was not found",
				    name
				);
				isError = true;
				value = 1;
			} else {
				assume(sect->offset == 0);
				value = sect->org;
			}
			break;
		}

		case RPN_SIZEOF_SECTTYPE:
			value = getRPNByte(expression, size, patch);
			if (value < 0 || value >= SECTTYPE_INVALID) {
				error(patch.src, patch.lineNo, "Requested SIZEOF() an invalid section type");
				isError = true;
				value = 0;
			} else {
				value = sectionTypeInfo[value].size;
			}
			break;

		case RPN_STARTOF_SECTTYPE:
			value = getRPNByte(expression, size, patch);
			if (value < 0 || value >= SECTTYPE_INVALID) {
				error(patch.src, patch.lineNo, "Requested STARTOF() an invalid section type");
				isError = true;
				value = 0;
			} else {
				value = sectionTypeInfo[value].startAddr;
			}
			break;

		case RPN_HRAM:
			value = popRPN(patch);
			if (!isError && (value < 0 || (value > 0xFF && value < 0xFF00) || value > 0xFFFF)) {
				error(patch.src, patch.lineNo, "Value %" PRId32 " is not in HRAM range", value);
				isError = true;
			}
			value &= 0xFF;
			break;

		case RPN_RST:
			value = popRPN(patch);
			// Acceptable values are 0x00, 0x08, 0x10, ..., 0x38
			// They can be easily checked with a bitmask
			if (value & ~0x38) {
				if (!isError)
					error(patch.src, patch.lineNo, "Value %" PRId32 " is not a RST vector", value);
				isError = true;
			}
			value |= 0xC7;
			break;

		case RPN_CONST:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(expression, size, patch) << shift;
			break;

		case RPN_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8)
				value |= getRPNByte(expression, size, patch) << shift;

			if (value == -1) { // PC
				if (!patch.pcSection) {
					error(patch.src, patch.lineNo, "PC has no value outside a section");
					value = 0;
					isError = true;
				} else {
					value = patch.pcOffset + patch.pcSection->org;
				}
			} else {
				if (Symbol const *symbol = getSymbol(fileSymbols, value); !symbol) {
					error(
					    patch.src,
					    patch.lineNo,
					    "Unknown symbol \"%s\"",
					    fileSymbols[value].name.c_str()
					);
					isError = true;
				} else if (auto *label = std::get_if<Label>(&symbol->data); label) {
					value = label->section->org + label->offset;
				} else {
					assume(std::holds_alternative<int32_t>(symbol->data));
					value = std::get<int32_t>(symbol->data);
				}
			}
			break;
		}

		pushRPN(value, isError);
	}

	if (rpnStack.size() > 1)
		error(patch.src, patch.lineNo, "RPN stack has %zu entries on exit, not 1", rpnStack.size());

	isError = false;
	return popRPN(patch);
}

void patch_CheckAssertions() {
	verbosePrint("Checking assertions...\n");

	for (Assertion &assert : assertions) {
		int32_t value = computeRPNExpr(assert.patch, *assert.fileSymbols);
		AssertionType type = (AssertionType)assert.patch.type;

		if (!isError && !value) {
			switch (type) {
			case ASSERT_FATAL:
				fatal(
				    assert.patch.src,
				    assert.patch.lineNo,
				    "%s",
				    !assert.message.empty() ? assert.message.c_str() : "assert failure"
				);
			case ASSERT_ERROR:
				error(
				    assert.patch.src,
				    assert.patch.lineNo,
				    "%s",
				    !assert.message.empty() ? assert.message.c_str() : "assert failure"
				);
				break;
			case ASSERT_WARN:
				warning(
				    assert.patch.src,
				    assert.patch.lineNo,
				    "%s",
				    !assert.message.empty() ? assert.message.c_str() : "assert failure"
				);
				break;
			}
		} else if (isError && type == ASSERT_FATAL) {
			fatal(
			    assert.patch.src,
			    assert.patch.lineNo,
			    "Failed to evaluate assertion%s%s",
			    !assert.message.empty() ? ": " : "",
			    assert.message.c_str()
			);
		}
	}
}

/*
 * Applies all of a section's patches
 * @param section The section component to patch
 * @param dataSection The section to patch
 */
static void applyFilePatches(Section &section, Section &dataSection) {
	verbosePrint("Patching section \"%s\"...\n", section.name.c_str());
	for (Patch &patch : section.patches) {
		int32_t value = computeRPNExpr(patch, *section.fileSymbols);
		uint16_t offset = patch.offset + section.offset;

		// `jr` is quite unlike the others...
		if (patch.type == PATCHTYPE_JR) {
			// Offset is relative to the byte *after* the operand
			// PC as operand to `jr` is lower than reference PC by 2
			uint16_t address = patch.pcSection->org + patch.pcOffset + 2;
			int16_t jumpOffset = value - address;

			if (!isError && (jumpOffset < -128 || jumpOffset > 127))
				error(
				    patch.src,
				    patch.lineNo,
				    "jr target must be between -128 and 127 bytes away, not %" PRId16
				    "; use jp instead\n",
				    jumpOffset
				);
			dataSection.data[offset] = jumpOffset & 0xFF;
		} else {
			// Patch a certain number of bytes
			struct {
				uint8_t size;
				int32_t min;
				int32_t max;
			} const types[PATCHTYPE_INVALID] = {
			    {1, -128,      255      }, // PATCHTYPE_BYTE
			    {2, -32768,    65536    }, // PATCHTYPE_WORD
			    {4, INT32_MIN, INT32_MAX}, // PATCHTYPE_LONG
			};

			if (!isError && (value < types[patch.type].min || value > types[patch.type].max))
				error(
				    patch.src,
				    patch.lineNo,
				    "Value %" PRId32 "%s is not %u-bit",
				    value,
				    value < 0 ? " (maybe negative?)" : "",
				    types[patch.type].size * 8U
				);
			for (uint8_t i = 0; i < types[patch.type].size; i++) {
				dataSection.data[offset + i] = value & 0xFF;
				value >>= 8;
			}
		}
	}
}

/*
 * Applies all of a section's patches, iterating over "components" of unionized sections
 * @param section The section to patch
 */
static void applyPatches(Section &section) {
	if (!sect_HasData(section.type))
		return;

	for (Section *component = &section; component; component = component->nextu.get())
		applyFilePatches(*component, section);
}

void patch_ApplyPatches() {
	sect_ForEach(applyPatches);
}
