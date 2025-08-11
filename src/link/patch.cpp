// SPDX-License-Identifier: MIT

#include "link/patch.hpp"

#include <deque>
#include <inttypes.h>
#include <stdint.h>
#include <vector>

#include "helpers.hpp" // assume
#include "linkdefs.hpp"
#include "opmath.hpp"
#include "verbosity.hpp"

#include "link/main.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"
#include "link/warning.hpp"

static std::deque<Assertion> assertions;

struct RPNStackEntry {
	int32_t value;
	bool errorFlag; // Whether the value is a placeholder inserted for error recovery
};

static std::deque<RPNStackEntry> rpnStack;

static void pushRPN(int32_t value, bool comesFromError) {
	rpnStack.push_front({.value = value, .errorFlag = comesFromError});
}

// This flag tracks whether the RPN op that is currently being evaluated
// has popped any values with the error flag set.
static bool isError = false;

#define diagnosticAt(patch, id, ...) \
	do { \
		bool errorDiag = warnings.getWarningBehavior(id) == WarningBehavior::ERROR; \
		if (!isError || !errorDiag) { \
			warningAt(patch, id, __VA_ARGS__); \
		} \
		if (errorDiag) { \
			isError = true; \
		} \
	} while (0)

#define firstErrorAt(...) \
	do { \
		if (!isError) { \
			errorAt(__VA_ARGS__); \
			isError = true; \
		} \
	} while (0)

static int32_t popRPN(Patch const &patch) {
	if (rpnStack.empty()) {
		fatalAt(patch, "Internal error, RPN stack empty");
	}

	RPNStackEntry entry = rpnStack.front();

	rpnStack.pop_front();
	isError |= entry.errorFlag;
	return entry.value;
}

// RPN operators

static uint32_t getRPNByte(uint8_t const *&expression, int32_t &size, Patch const &patch) {
	if (!size--) {
		fatalAt(patch, "Internal error, RPN expression overread");
	}

	return *expression++;
}

static Symbol const *getSymbol(std::vector<Symbol> const &symbolList, uint32_t index) {
	assume(index != UINT32_MAX); // PC needs to be handled specially, not here
	Symbol const &symbol = symbolList[index];

	// If the symbol is defined elsewhere...
	if (symbol.type == SYMTYPE_IMPORT) {
		return sym_GetSymbol(symbol.name);
	}

	return &symbol;
}

// Compute a patch's value from its RPN string.
static int32_t computeRPNExpr(Patch const &patch, std::vector<Symbol> const &fileSymbols) {
	uint8_t const *expression = patch.rpnExpression.data();
	int32_t size = static_cast<int32_t>(patch.rpnExpression.size());

	rpnStack.clear();

	while (size > 0) {
		RPNCommand command = static_cast<RPNCommand>(getRPNByte(expression, size, patch));

		isError = false;

		// Be VERY careful with two `popRPN` in the same expression.
		// C++ does not guarantee order of evaluation of operands!
		// So, if there are two `popRPN` in the same expression, make
		// sure the operation is commutative.
		int32_t value;
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
				firstErrorAt(patch, "Division by 0");
				popRPN(patch);
				value = 0;
			} else if (int32_t lval = popRPN(patch); lval == INT32_MIN && value == -1) {
				diagnosticAt(
				    patch,
				    WARNING_DIV,
				    "Division of %" PRId32 " by -1 yields %" PRId32,
				    INT32_MIN,
				    INT32_MIN
				);
				value = INT32_MIN;
			} else {
				value = op_divide(lval, value);
			}
			break;
		case RPN_MOD:
			value = popRPN(patch);
			if (value == 0) {
				firstErrorAt(patch, "Modulo by 0");
				popRPN(patch);
				value = 0;
			} else {
				value = op_modulo(popRPN(patch), value);
			}
			break;
		case RPN_NEG:
			value = op_neg(popRPN(patch));
			break;
		case RPN_EXP:
			value = popRPN(patch);
			if (value < 0) {
				firstErrorAt(patch, "Exponent by negative value %" PRId32, value);
				popRPN(patch);
				value = 0;
			} else {
				value = op_exponent(popRPN(patch), value);
			}
			break;

		case RPN_HIGH:
			value = op_high(popRPN(patch));
			break;
		case RPN_LOW:
			value = op_low(popRPN(patch));
			break;

		case RPN_BITWIDTH:
			value = op_bitwidth(popRPN(patch));
			break;
		case RPN_TZCOUNT:
			value = op_tzcount(popRPN(patch));
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
			if (value < 0) {
				diagnosticAt(
				    patch, WARNING_SHIFT_AMOUNT, "Shifting left by negative amount %" PRId32, value
				);
			}
			if (value >= 32) {
				diagnosticAt(
				    patch, WARNING_SHIFT_AMOUNT, "Shifting left by large amount %" PRId32, value
				);
			}
			value = op_shift_left(popRPN(patch), value);
			break;
		case RPN_SHR: {
			value = popRPN(patch);
			int32_t lval = popRPN(patch);
			if (lval < 0) {
				diagnosticAt(patch, WARNING_SHIFT, "Shifting right negative value %" PRId32, lval);
			}
			if (value < 0) {
				diagnosticAt(
				    patch, WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32, value
				);
			}
			if (value >= 32) {
				diagnosticAt(
				    patch, WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32, value
				);
			}
			value = op_shift_right(lval, value);
			break;
		}
		case RPN_USHR:
			value = popRPN(patch);
			if (value < 0) {
				diagnosticAt(
				    patch, WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32, value
				);
			}
			if (value >= 32) {
				diagnosticAt(
				    patch, WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32, value
				);
			}
			value = op_shift_right_unsigned(popRPN(patch), value);
			break;

		case RPN_BANK_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8) {
				value |= getRPNByte(expression, size, patch) << shift;
			}

			if (Symbol const *symbol = getSymbol(fileSymbols, value); !symbol) {
				errorAt(
				    patch,
				    "Requested BANK() of undefined symbol \"%s\"",
				    fileSymbols[value].name.c_str()
				);
				isError = true;
				value = 1;
			} else if (std::holds_alternative<Label>(symbol->data)) {
				value = std::get<Label>(symbol->data).section->bank;
			} else {
				errorAt(
				    patch,
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
			char const *name = reinterpret_cast<char const *>(expression);
			while (getRPNByte(expression, size, patch)) {}

			if (Section const *sect = sect_GetSection(name); !sect) {
				errorAt(patch, "Requested BANK() of undefined section \"%s\"", name);
				isError = true;
				value = 1;
			} else {
				value = sect->bank;
			}
			break;
		}

		case RPN_BANK_SELF:
			if (!patch.pcSection) {
				errorAt(patch, "PC has no bank outside of a section");
				isError = true;
				value = 1;
			} else {
				value = patch.pcSection->bank;
			}
			break;

		case RPN_SIZEOF_SECT: {
			// This has assumptions commented in the `RPN_BANK_SECT` case above.
			char const *name = reinterpret_cast<char const *>(expression);
			while (getRPNByte(expression, size, patch)) {}

			if (Section const *sect = sect_GetSection(name); !sect) {
				errorAt(patch, "Requested SIZEOF() of undefined section \"%s\"", name);
				isError = true;
				value = 1;
			} else {
				value = sect->size;
			}
			break;
		}

		case RPN_STARTOF_SECT: {
			// This has assumptions commented in the `RPN_BANK_SECT` case above.
			char const *name = reinterpret_cast<char const *>(expression);
			while (getRPNByte(expression, size, patch)) {}

			if (Section const *sect = sect_GetSection(name); !sect) {
				errorAt(patch, "Requested STARTOF() of undefined section \"%s\"", name);
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
				errorAt(patch, "Requested SIZEOF() an invalid section type");
				isError = true;
				value = 0;
			} else {
				value = sectionTypeInfo[value].size;
			}
			break;

		case RPN_STARTOF_SECTTYPE:
			value = getRPNByte(expression, size, patch);
			if (value < 0 || value >= SECTTYPE_INVALID) {
				errorAt(patch, "Requested STARTOF() an invalid section type");
				isError = true;
				value = 0;
			} else {
				value = sectionTypeInfo[value].startAddr;
			}
			break;

		case RPN_HRAM:
			value = popRPN(patch);
			if (value < 0xFF00 || value > 0xFFFF) {
				firstErrorAt(
				    patch,
				    "Address $%" PRIx32 " for LDH is not in HRAM range; use LD instead",
				    value
				);
				value = 0;
			}
			value &= 0xFF;
			break;

		case RPN_RST:
			value = popRPN(patch);
			// Acceptable values are 0x00, 0x08, 0x10, ..., 0x38
			if (value & ~0x38) {
				firstErrorAt(
				    patch, "Value $%" PRIx32 " is not a RST vector; use CALL instead", value
				);
				value = 0;
			}
			value |= 0xC7;
			break;

		case RPN_BIT_INDEX: {
			value = popRPN(patch);
			int32_t mask = getRPNByte(expression, size, patch);
			// Acceptable values are 0 to 7
			if (value & ~0x07) {
				firstErrorAt(patch, "Value $%" PRIx32 " is not a bit index", value);
				value = 0;
			}
			value = mask | (value << 3);
			break;
		}

		case RPN_CONST:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8) {
				value |= getRPNByte(expression, size, patch) << shift;
			}
			break;

		case RPN_SYM:
			value = 0;
			for (uint8_t shift = 0; shift < 32; shift += 8) {
				value |= getRPNByte(expression, size, patch) << shift;
			}

			if (value == -1) { // PC
				if (patch.pcSection) {
					value = patch.pcOffset + patch.pcSection->org;
				} else {
					errorAt(patch, "PC has no value outside of a section");
					value = 0;
					isError = true;
				}
			} else if (Symbol const *symbol = getSymbol(fileSymbols, value); !symbol) {
				errorAt(patch, "Undefined symbol \"%s\"", fileSymbols[value].name.c_str());
				sym_TraceLocalAliasedSymbols(fileSymbols[value].name);
				isError = true;
			} else if (std::holds_alternative<Label>(symbol->data)) {
				Label const &label = std::get<Label>(symbol->data);
				value = label.section->org + label.offset;
			} else {
				value = std::get<int32_t>(symbol->data);
			}
			break;
		}

		pushRPN(value, isError);
	}

	if (rpnStack.size() > 1) {
		errorAt(patch, "RPN stack has %zu entries on exit, not 1", rpnStack.size());
	}

	isError = false;
	return popRPN(patch);
}

Assertion &patch_AddAssertion() {
	return assertions.emplace_front();
}

void patch_CheckAssertions() {
	verbosePrint(VERB_NOTICE, "Checking assertions...\n");

	for (Assertion &assert : assertions) {
		int32_t value = computeRPNExpr(assert.patch, *assert.fileSymbols);
		AssertionType type = static_cast<AssertionType>(assert.patch.type);

		if (!isError && !value) {
			switch (type) {
			case ASSERT_FATAL:
				fatalAt(
				    assert.patch,
				    "%s",
				    !assert.message.empty() ? assert.message.c_str() : "assert failure"
				);
			case ASSERT_ERROR:
				errorAt(
				    assert.patch,
				    "%s",
				    !assert.message.empty() ? assert.message.c_str() : "assert failure"
				);
				break;
			case ASSERT_WARN:
				warningAt(
				    assert.patch,
				    WARNING_ASSERT,
				    "%s",
				    !assert.message.empty() ? assert.message.c_str() : "assert failure"
				);
				break;
			}
		} else if (isError && type == ASSERT_FATAL) {
			fatalAt(
			    assert.patch,
			    "Failed to evaluate assertion%s%s",
			    !assert.message.empty() ? ": " : "",
			    assert.message.c_str()
			);
		}
	}
}

// Applies all of a section's patches to a data section
static void applyFilePatches(Section &section, Section &dataSection) {
	verbosePrint(VERB_INFO, "Patching section \"%s\"...\n", section.name.c_str());
	for (Patch &patch : section.patches) {
		int32_t value = computeRPNExpr(patch, *section.fileSymbols);
		uint16_t offset = patch.offset + section.offset;

		struct {
			uint8_t size;
			int32_t min;
			int32_t max;
		} const types[PATCHTYPE_INVALID] = {
		    {1, -128,      255      }, // PATCHTYPE_BYTE
		    {2, -32768,    65536    }, // PATCHTYPE_WORD
		    {4, INT32_MIN, INT32_MAX}, // PATCHTYPE_LONG
		    {1, 0,         0        }, // PATCHTYPE_JR
		};
		auto const &type = types[patch.type];

		if (dataSection.data.size() < offset + type.size) {
			errorAt(
			    patch,
			    "Patch would write %zu bytes past the end of section \"%s\" (%zu bytes long)",
			    offset + type.size - dataSection.data.size(),
			    dataSection.name.c_str(),
			    dataSection.data.size()
			);
		} else if (patch.type == PATCHTYPE_JR) { // `jr` is quite unlike the others...
			// Offset is relative to the byte *after* the operand
			// PC as operand to `jr` is lower than reference PC by 2
			uint16_t address = patch.pcSection->org + patch.pcOffset + 2;
			int16_t jumpOffset = value - address;

			if (jumpOffset < -128 || jumpOffset > 127) {
				firstErrorAt(
				    patch,
				    "JR target must be between -128 and 127 bytes away, not %" PRId16
				    "; use JP instead",
				    jumpOffset
				);
			}
			dataSection.data[offset] = jumpOffset & 0xFF;
		} else {
			// Patch a certain number of bytes
			if (value < type.min || value > type.max) {
				diagnosticAt(
				    patch,
				    WARNING_TRUNCATION,
				    "Value $%" PRIx32 "%s is not %u-bit",
				    value,
				    value < 0 ? " (may be negative?)" : "",
				    type.size * 8U
				);
			}
			for (uint8_t i = 0; i < type.size; ++i) {
				dataSection.data[offset + i] = value & 0xFF;
				value >>= 8;
			}
		}
	}
}

// Applies all of a section's patches, iterating over "components" of unionized sections
static void applyPatches(Section &section) {
	if (!sect_HasData(section.type)) {
		return;
	}

	for (Section *component = &section; component; component = component->nextu.get()) {
		applyFilePatches(*component, section);
	}
}

void patch_ApplyPatches() {
	sect_ForEach(applyPatches);
}
