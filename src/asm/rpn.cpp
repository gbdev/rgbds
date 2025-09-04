// SPDX-License-Identifier: MIT

#include "asm/rpn.hpp"

#include <inttypes.h>
#include <limits.h>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "helpers.hpp" // assume
#include "linkdefs.hpp"
#include "opmath.hpp"

#include "asm/output.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

using namespace std::literals;

int32_t Expression::getConstVal() const {
	if (!isKnown()) {
		error("Expected constant expression: %s", std::get<std::string>(data).c_str());
		return 0;
	}
	return value();
}

Symbol const *Expression::symbolOf() const {
	if (rpn.size() != 1 || rpn[0].command != RPN_SYM) {
		return nullptr;
	}
	return sym_FindScopedSymbol(std::get<std::string>(rpn[0].data));
}

bool Expression::isDiffConstant(Symbol const *sym) const {
	// Check if both expressions only refer to a single symbol
	Symbol const *sym1 = symbolOf();

	if (!sym1 || !sym || sym1->type != SYM_LABEL || sym->type != SYM_LABEL) {
		return false;
	}

	Section const *sect1 = sym1->getSection();
	Section const *sect2 = sym->getSection();
	return sect1 && (sect1 == sect2);
}

void Expression::makeNumber(uint32_t value) {
	assume(rpn.empty());
	data = static_cast<int32_t>(value);
}

void Expression::makeSymbol(std::string const &symName) {
	assume(rpn.empty());
	if (Symbol *sym = sym_FindScopedSymbol(symName); sym_IsPC(sym) && !sect_GetSymbolSection()) {
		error("PC has no value outside of a section");
		data = 0;
	} else if (sym && !sym->isNumeric() && !sym->isLabel()) {
		error("`%s` is not a numeric symbol", symName.c_str());
		data = 0;
	} else if (!sym || !sym->isConstant()) {
		data = sym_IsPC(sym) ? "PC is not constant at assembly time"
		                     : (sym && sym->isDefined()
		                            ? "`"s + symName + "` is not constant at assembly time"
		                            : "undefined symbol `"s + symName + "`")
		                           + (sym_IsPurgedScoped(symName) ? "; it was purged" : "");
		sym = sym_Ref(symName);
		rpn.push_back({.command = RPN_SYM, .data = sym->name});
	} else {
		data = static_cast<int32_t>(sym->getConstantValue());
	}
}

void Expression::makeBankSymbol(std::string const &symName) {
	assume(rpn.empty());
	if (Symbol const *sym = sym_FindScopedSymbol(symName); sym_IsPC(sym)) {
		// The @ symbol is treated differently.
		if (std::optional<uint32_t> outputBank = sect_GetOutputBank(); !outputBank) {
			error("PC has no bank outside of a section");
			data = 1;
		} else if (*outputBank == UINT32_MAX) {
			data = "Current section's bank is not known";
			rpn.push_back({.command = RPN_BANK_SELF, .data = std::monostate{}});
		} else {
			data = static_cast<int32_t>(*outputBank);
		}
	} else if (sym && !sym->isLabel()) {
		error("`BANK` argument must be a label");
		data = 1;
	} else {
		sym = sym_Ref(symName);
		assume(sym); // If the symbol didn't exist, it should have been created
		if (sym->getSection() && sym->getSection()->bank != UINT32_MAX) {
			// Symbol's section is known and bank is fixed
			data = static_cast<int32_t>(sym->getSection()->bank);
		} else {
			data = sym_IsPurgedScoped(symName)
			           ? "`"s + symName + "`'s bank is not known; it was purged"
			           : "`"s + symName + "`'s bank is not known";
			rpn.push_back({.command = RPN_BANK_SYM, .data = sym->name});
		}
	}
}

void Expression::makeBankSection(std::string const &sectName) {
	assume(rpn.empty());
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->bank != UINT32_MAX) {
		data = static_cast<int32_t>(sect->bank);
	} else {
		data = "Section \""s + sectName + "\"'s bank is not known";
		rpn.push_back({.command = RPN_BANK_SECT, .data = sectName});
	}
}

void Expression::makeSizeOfSection(std::string const &sectName) {
	assume(rpn.empty());
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->isSizeKnown()) {
		data = static_cast<int32_t>(sect->size);
	} else {
		data = "Section \""s + sectName + "\"'s size is not known";
		rpn.push_back({.command = RPN_SIZEOF_SECT, .data = sectName});
	}
}

void Expression::makeStartOfSection(std::string const &sectName) {
	assume(rpn.empty());
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->org != UINT32_MAX) {
		data = static_cast<int32_t>(sect->org);
	} else {
		data = "Section \""s + sectName + "\"'s start is not known";
		rpn.push_back({.command = RPN_STARTOF_SECT, .data = sectName});
	}
}

void Expression::makeSizeOfSectionType(SectionType type) {
	assume(rpn.empty());
	data = "Section type's size is not known";
	rpn.push_back({.command = RPN_SIZEOF_SECTTYPE, .data = static_cast<uint8_t>(type)});
}

void Expression::makeStartOfSectionType(SectionType type) {
	assume(rpn.empty());
	data = "Section type's start is not known";
	rpn.push_back({.command = RPN_STARTOF_SECTTYPE, .data = static_cast<uint8_t>(type)});
}

static bool tryConstZero(Expression const &lhs, Expression const &rhs) {
	Expression const &expr = lhs.isKnown() ? lhs : rhs;
	return expr.isKnown() && expr.value() == 0;
}

static bool tryConstNonzero(Expression const &lhs, Expression const &rhs) {
	Expression const &expr = lhs.isKnown() ? lhs : rhs;
	return expr.isKnown() && expr.value() != 0;
}

static bool tryConstLogNot(Expression const &expr) {
	Symbol const *sym = expr.symbolOf();
	if (!sym || !sym->getSection() || !sym->isDefined()) {
		return false;
	}

	assume(sym->isNumeric());

	Section const &sect = *sym->getSection();
	int32_t unknownBits = (1 << 16) - (1 << sect.align);

	// `sym->getValue()` attempts to add the section's address, but that's `UINT32_MAX`
	// because the section is floating (otherwise we wouldn't be here)
	assume(sect.org == UINT32_MAX);
	int32_t symbolOfs = sym->getValue() + 1;

	int32_t knownBits = (symbolOfs + sect.alignOfs) & ~unknownBits;
	return knownBits != 0;
}

// Returns a constant LOW() from non-constant argument, or -1 if it cannot be computed.
// This is possible if the argument is a symbol belonging to an `ALIGN[8]` section.
static int32_t tryConstLow(Expression const &expr) {
	Symbol const *sym = expr.symbolOf();
	if (!sym || !sym->getSection() || !sym->isDefined()) {
		return -1;
	}

	assume(sym->isNumeric());

	// The low byte must not cover any unknown bits
	Section const &sect = *sym->getSection();
	if (sect.align < 8) {
		return -1;
	}

	// `sym->getValue()` attempts to add the section's address, but that's `UINT32_MAX`
	// because the section is floating (otherwise we wouldn't be here)
	assume(sect.org == UINT32_MAX);
	int32_t symbolOfs = sym->getValue() + 1;

	return op_low(symbolOfs + sect.alignOfs);
}

// Returns a constant binary AND with one non-constant operand, or -1 if it cannot be computed.
// This is possible if one operand is a symbol belonging to an `ALIGN[N]` section, and the other is
// a constant that only keeps (some of) the lower N bits.
static int32_t tryConstMask(Expression const &lhs, Expression const &rhs) {
	Symbol const *lhsSymbol = lhs.symbolOf();
	Symbol const *rhsSymbol = lhsSymbol ? nullptr : rhs.symbolOf();
	bool lhsIsSymbol = lhsSymbol && lhsSymbol->getSection();
	bool rhsIsSymbol = rhsSymbol && rhsSymbol->getSection();

	if (!lhsIsSymbol && !rhsIsSymbol) {
		return -1;
	}

	// If the lhs isn't a symbol, try again the other way around
	Symbol const &sym = lhsIsSymbol ? *lhsSymbol : *rhsSymbol;
	Expression const &expr = lhsIsSymbol ? rhs : lhs; // Opposite side of `sym`

	if (!sym.isDefined() || !expr.isKnown()) {
		return -1;
	}

	assume(sym.isNumeric());

	// We can now safely use `expr.value()`
	int32_t mask = expr.value();

	// The mask must not cover any unknown bits
	Section const &sect = *sym.getSection();
	if (int32_t unknownBits = (1 << 16) - (1 << sect.align); (unknownBits & mask) != 0) {
		return -1;
	}

	// `sym.getValue()` attempts to add the section's address, but that's `UINT32_MAX`
	// because the section is floating (otherwise we wouldn't be here)
	assume(sect.org == UINT32_MAX);
	int32_t symbolOfs = sym.getValue() + 1;

	return (symbolOfs + sect.alignOfs) & mask;
}

void Expression::makeUnaryOp(RPNCommand op, Expression &&src) {
	assume(rpn.empty());
	// First, check if the expression is known
	if (src.isKnown()) {
		// If the expressions is known, just compute the value
		switch (int32_t val = src.value(); op) {
		case RPN_NEG:
			data = op_neg(val);
			break;
		case RPN_NOT:
			data = ~val;
			break;
		case RPN_LOGNOT:
			data = !val;
			break;
		case RPN_HIGH:
			data = op_high(val);
			break;
		case RPN_LOW:
			data = op_low(val);
			break;
		case RPN_BITWIDTH:
			data = op_bitwidth(val);
			break;
		case RPN_TZCOUNT:
			data = op_tzcount(val);
			break;
		// LCOV_EXCL_START
		default:
			// `makeUnaryOp` should never be called with a non-unary operator!
			unreachable_();
		}
		// LCOV_EXCL_STOP
	} else if (op == RPN_LOGNOT && tryConstLogNot(src)) {
		data = 0;
	} else if (int32_t constVal; op == RPN_LOW && (constVal = tryConstLow(src)) != -1) {
		data = constVal;
	} else {
		// If it's not known, just reuse its RPN vector and append the operator
		data = std::move(src.data);
		std::swap(rpn, src.rpn);
		rpn.push_back({.command = op, .data = std::monostate{}});
	}
}

void Expression::makeBinaryOp(RPNCommand op, Expression &&src1, Expression const &src2) {
	assume(rpn.empty());
	// First, check if the expressions are known
	if (src1.isKnown() && src2.isKnown()) {
		// If both expressions are known, just compute the value
		int32_t lval = src1.value(), rval = src2.value();
		uint32_t ulval = static_cast<uint32_t>(lval), urval = static_cast<uint32_t>(rval);

		switch (op) {
		case RPN_LOGOR:
			data = lval || rval;
			break;
		case RPN_LOGAND:
			data = lval && rval;
			break;
		case RPN_LOGEQ:
			data = lval == rval;
			break;
		case RPN_LOGGT:
			data = lval > rval;
			break;
		case RPN_LOGLT:
			data = lval < rval;
			break;
		case RPN_LOGGE:
			data = lval >= rval;
			break;
		case RPN_LOGLE:
			data = lval <= rval;
			break;
		case RPN_LOGNE:
			data = lval != rval;
			break;
		case RPN_ADD:
			data = static_cast<int32_t>(ulval + urval);
			break;
		case RPN_SUB:
			data = static_cast<int32_t>(ulval - urval);
			break;
		case RPN_XOR:
			data = lval ^ rval;
			break;
		case RPN_OR:
			data = lval | rval;
			break;
		case RPN_AND:
			data = lval & rval;
			break;
		case RPN_SHL:
			if (rval < 0) {
				warning(WARNING_SHIFT_AMOUNT, "Shifting left by negative amount %" PRId32, rval);
			}
			if (rval >= 32) {
				warning(WARNING_SHIFT_AMOUNT, "Shifting left by large amount %" PRId32, rval);
			}
			data = op_shift_left(lval, rval);
			break;
		case RPN_SHR:
			if (lval < 0) {
				warning(WARNING_SHIFT, "Shifting right negative value %" PRId32, lval);
			}
			if (rval < 0) {
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32, rval);
			}
			if (rval >= 32) {
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32, rval);
			}
			data = op_shift_right(lval, rval);
			break;
		case RPN_USHR:
			if (rval < 0) {
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32, rval);
			}
			if (rval >= 32) {
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32, rval);
			}
			data = op_shift_right_unsigned(lval, rval);
			break;
		case RPN_MUL:
			data = static_cast<int32_t>(ulval * urval);
			break;
		case RPN_DIV:
			if (rval == 0) {
				fatal("Division by zero");
			}
			if (lval == INT32_MIN && rval == -1) {
				warning(
				    WARNING_DIV,
				    "Division of %" PRId32 " by -1 yields %" PRId32,
				    INT32_MIN,
				    INT32_MIN
				);
				data = INT32_MIN;
			} else {
				data = op_divide(lval, rval);
			}
			break;
		case RPN_MOD:
			if (rval == 0) {
				fatal("Modulo by zero");
			}
			if (lval == INT32_MIN && rval == -1) {
				data = 0;
			} else {
				data = op_modulo(lval, rval);
			}
			break;
		case RPN_EXP:
			if (rval < 0) {
				fatal("Exponentiation by negative power");
			}
			data = op_exponent(lval, rval);
			break;
		// LCOV_EXCL_START
		default:
			// `makeBinaryOp` should never be called with a non-binary operator!
			unreachable_();
		}
		// LCOV_EXCL_STOP
	} else if (op == RPN_SUB && src1.isDiffConstant(src2.symbolOf())) {
		data = src1.symbolOf()->getValue() - src2.symbolOf()->getValue();
	} else if ((op == RPN_LOGAND || op == RPN_AND) && tryConstZero(src1, src2)) {
		data = 0;
	} else if (op == RPN_LOGOR && tryConstNonzero(src1, src2)) {
		data = 1;
	} else if (int32_t constVal; op == RPN_AND && (constVal = tryConstMask(src1, src2)) != -1) {
		data = constVal;
	} else {
		// If it's not known, start computing the RPN expression

		// Convert the left-hand expression if it's constant
		if (src1.isKnown()) {
			uint32_t lval = src1.value();
			// Use the other expression's un-const reason
			data = std::move(src2.data);
			rpn.push_back({.command = RPN_CONST, .data = lval});
		} else {
			// Otherwise just reuse its RPN vector
			data = std::move(src1.data);
			std::swap(rpn, src1.rpn);
		}

		// Now, merge the right expression into the left one
		if (src2.isKnown()) {
			// If the right expression is constant, append its value
			uint32_t rval = src2.value();
			rpn.push_back({.command = RPN_CONST, .data = rval});
		} else {
			// Otherwise just extend with its RPN vector
			rpn.insert(rpn.end(), RANGE(src2.rpn));
		}
		// Append the operator
		rpn.push_back({.command = op, .data = std::monostate{}});
	}
}

void Expression::addCheckHRAM() {
	if (!isKnown()) {
		rpn.push_back({.command = RPN_HRAM, .data = std::monostate{}});
	} else if (int32_t val = value(); val >= 0xFF00 && val <= 0xFFFF) {
		// That range is valid; only keep the lower byte
		data = val & 0xFF;
	} else {
		error("Source address $%" PRIx32 " not between $FF00 to $FFFF", val);
	}
}

void Expression::addCheckRST() {
	if (!isKnown()) {
		rpn.push_back({.command = RPN_RST, .data = std::monostate{}});
	} else if (int32_t val = value(); val & ~0x38) {
		// A valid RST address must be masked with 0x38
		error("Invalid address $%" PRIx32 " for `RST`", val);
	}
}

void Expression::addCheckBitIndex(uint8_t mask) {
	assume((mask & 0xC0) != 0x00); // The high two bits must correspond to BIT, RES, or SET
	if (!isKnown()) {
		rpn.push_back({.command = RPN_BIT_INDEX, .data = mask});
	} else if (int32_t val = value(); val & ~0x07) {
		// A valid bit index must be masked with 0x07
		static char const *instructions[4] = {"instruction", "`BIT`", "`RES`", "`SET`"};
		error("Invalid bit index %" PRId32 " for %s", val, instructions[mask >> 6]);
	}
}

// Checks that an RPN expression's value fits within N bits (signed or unsigned)
void Expression::checkNBit(uint8_t n) const {
	if (isKnown()) {
		::checkNBit(value(), n, nullptr);
	}
}

bool checkNBit(int32_t v, uint8_t n, char const *name) {
	assume(n != 0);                     // That doesn't make sense
	assume(n < CHAR_BIT * sizeof(int)); // Otherwise `1 << n` is UB

	if (v < -(1 << n) || v >= 1 << n) {
		warning(
		    WARNING_TRUNCATION_1,
		    "%s must be %u-bit%s",
		    name ? name : "Expression",
		    n,
		    n == 8 && !name ? "; use `LOW()` to force 8-bit" : ""
		);
		return false;
	}
	if (v < -(1 << (n - 1))) {
		warning(
		    WARNING_TRUNCATION_2,
		    "%s must be %u-bit%s",
		    name ? name : "Expression",
		    n,
		    n == 8 && !name ? "; use `LOW()` to force 8-bit" : ""
		);
		return false;
	}

	return true;
}

void Expression::encode(std::vector<uint8_t> &buffer) const {
	assume(buffer.empty());

	if (isKnown()) {
		// If the RPN expression's value is known, output a constant directly
		uint32_t val = value();
		buffer.resize(5);
		buffer[0] = RPN_CONST;
		buffer[1] = val & 0xFF;
		buffer[2] = val >> 8;
		buffer[3] = val >> 16;
		buffer[4] = val >> 24;
	} else {
		// If the RPN expression's value is not known, serialize its RPN values
		buffer.reserve(rpn.size() * 2); // Rough estimate of the serialized size
		for (RPNValue const &val : rpn) {
			val.appendEncoded(buffer);
		}
	}
}

void RPNValue::appendEncoded(std::vector<uint8_t> &buffer) const {
	// Every command starts with its own ID
	buffer.push_back(command);

	switch (command) {
	case RPN_CONST: {
		// The command ID is followed by a four-byte integer
		assume(std::holds_alternative<uint32_t>(data));
		uint32_t val = std::get<uint32_t>(data);
		buffer.push_back(val & 0xFF);
		buffer.push_back(val >> 8);
		buffer.push_back(val >> 16);
		buffer.push_back(val >> 24);
		break;
	}

	case RPN_SYM:
	case RPN_BANK_SYM: {
		// The command ID is followed by a four-byte symbol ID
		assume(std::holds_alternative<std::string>(data));
		// The symbol name is always written expanded
		Symbol *sym = sym_FindExactSymbol(std::get<std::string>(data));
		out_RegisterSymbol(*sym); // Ensure that `sym->ID` is set
		buffer.push_back(sym->ID & 0xFF);
		buffer.push_back(sym->ID >> 8);
		buffer.push_back(sym->ID >> 16);
		buffer.push_back(sym->ID >> 24);
		break;
	}

	case RPN_BANK_SECT:
	case RPN_SIZEOF_SECT:
	case RPN_STARTOF_SECT: {
		// The command ID is followed by a NUL-terminated section name string
		assume(std::holds_alternative<std::string>(data));
		std::string const &name = std::get<std::string>(data);
		buffer.reserve(buffer.size() + name.length() + 1);
		buffer.insert(buffer.end(), RANGE(name));
		buffer.push_back('\0');
		break;
	}

	case RPN_SIZEOF_SECTTYPE:
	case RPN_STARTOF_SECTTYPE:
	case RPN_BIT_INDEX:
		// The command ID is followed by a byte value
		assume(std::holds_alternative<uint8_t>(data));
		buffer.push_back(std::get<uint8_t>(data));
		break;

	default:
		// Other command IDs are not followed by anything
		assume(std::holds_alternative<std::monostate>(data));
		break;
	}
}
