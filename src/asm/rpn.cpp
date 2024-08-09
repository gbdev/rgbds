/* SPDX-License-Identifier: MIT */

#include "asm/rpn.hpp"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>

#include "helpers.hpp" // assume, clz, ctz
#include "opmath.hpp"

#include "asm/output.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

using namespace std::literals;

int32_t Expression::value() const {
	assume(std::holds_alternative<int32_t>(data));
	return std::get<int32_t>(data);
}

void Expression::clear() {
	data = 0;
	isSymbol = false;
	rpn.clear();
	rpnPatchSize = 0;
}

uint8_t *Expression::reserveSpace(uint32_t size) {
	return reserveSpace(size, size);
}

uint8_t *Expression::reserveSpace(uint32_t size, uint32_t patchSize) {
	rpnPatchSize += patchSize;
	size_t curSize = rpn.size();
	rpn.resize(curSize + size);
	return &rpn[curSize];
}

int32_t Expression::getConstVal() const {
	if (!isKnown()) {
		error("Expected constant expression: %s\n", std::get<std::string>(data).c_str());
		return 0;
	}
	return value();
}

Symbol const *Expression::symbolOf() const {
	if (!isSymbol)
		return nullptr;
	return sym_FindScopedSymbol((char const *)&rpn[1]);
}

bool Expression::isDiffConstant(Symbol const *sym) const {
	// Check if both expressions only refer to a single symbol
	Symbol const *sym1 = symbolOf();

	if (!sym1 || !sym || sym1->type != SYM_LABEL || sym->type != SYM_LABEL)
		return false;

	Section const *sect1 = sym1->getSection();
	Section const *sect2 = sym->getSection();
	return sect1 && (sect1 == sect2);
}

void Expression::makeNumber(uint32_t value) {
	clear();
	data = (int32_t)value;
}

void Expression::makeSymbol(std::string const &symName) {
	clear();
	if (Symbol *sym = sym_FindScopedSymbol(symName); sym_IsPC(sym) && !sect_GetSymbolSection()) {
		error("PC has no value outside a section\n");
		data = 0;
	} else if (!sym || !sym->isConstant()) {
		isSymbol = true;

		data = sym_IsPC(sym) ? "PC is not constant at assembly time"
		                     : sym_IsPurgedScoped(symName)
		                     ? "'"s + symName + "' is not constant at assembly time; it was purged"
		                     : "'"s + symName + "' is not constant at assembly time";
		sym = sym_Ref(symName);

		size_t nameLen = sym->name.length() + 1; // Don't forget NUL!

		// 1-byte opcode + 4-byte symbol ID
		uint8_t *ptr = reserveSpace(nameLen + 1, 5);
		*ptr++ = RPN_SYM;
		memcpy(ptr, sym->name.c_str(), nameLen);
	} else {
		data = (int32_t)sym_GetConstantValue(symName);
	}
}

void Expression::makeBankSymbol(std::string const &symName) {
	clear();
	if (Symbol const *sym = sym_FindScopedSymbol(symName); sym_IsPC(sym)) {
		// The @ symbol is treated differently.
		if (!currentSection) {
			error("PC has no bank outside a section\n");
			data = 1;
		} else if (currentSection->bank == (uint32_t)-1) {
			data = "Current section's bank is not known";

			*reserveSpace(1) = RPN_BANK_SELF;
		} else {
			data = (int32_t)currentSection->bank;
		}
		return;
	} else if (sym && !sym->isLabel()) {
		error("BANK argument must be a label\n");
		data = 1;
	} else {
		sym = sym_Ref(symName);
		assume(sym); // If the symbol didn't exist, it should have been created

		if (sym->getSection() && sym->getSection()->bank != (uint32_t)-1) {
			// Symbol's section is known and bank is fixed
			data = (int32_t)sym->getSection()->bank;
		} else {
			data = sym_IsPurgedScoped(symName)
			     ? "\""s + symName + "\"'s bank is not known; it was purged"
			     : "\""s + symName + "\"'s bank is not known";

			size_t nameLen = sym->name.length() + 1; // Room for NUL!

			// 1-byte opcode + 4-byte sect ID
			uint8_t *ptr = reserveSpace(nameLen + 1, 5);
			*ptr++ = RPN_BANK_SYM;
			memcpy(ptr, sym->name.c_str(), nameLen);
		}
	}
}

void Expression::makeBankSection(std::string const &sectName) {
	clear();
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->bank != (uint32_t)-1) {
		data = (int32_t)sect->bank;
	} else {
		data = "Section \""s + sectName + "\"'s bank is not known";

		size_t nameLen = sectName.length() + 1; // Room for NUL!

		uint8_t *ptr = reserveSpace(nameLen + 1);
		*ptr++ = RPN_BANK_SECT;
		memcpy(ptr, sectName.data(), nameLen);
	}
}

void Expression::makeSizeOfSection(std::string const &sectName) {
	clear();
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->isSizeKnown()) {
		data = (int32_t)sect->size;
	} else {
		data = "Section \""s + sectName + "\"'s size is not known";

		size_t nameLen = sectName.length() + 1; // Room for NUL!

		uint8_t *ptr = reserveSpace(nameLen + 1);
		*ptr++ = RPN_SIZEOF_SECT;
		memcpy(ptr, sectName.data(), nameLen);
	}
}

void Expression::makeStartOfSection(std::string const &sectName) {
	clear();
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->org != (uint32_t)-1) {
		data = (int32_t)sect->org;
	} else {
		data = "Section \""s + sectName + "\"'s start is not known";

		size_t nameLen = sectName.length() + 1; // Room for NUL!

		uint8_t *ptr = reserveSpace(nameLen + 1);
		*ptr++ = RPN_STARTOF_SECT;
		memcpy(ptr, sectName.data(), nameLen);
	}
}

void Expression::makeSizeOfSectionType(SectionType type) {
	clear();
	data = "Section type's size is not known";

	uint8_t *ptr = reserveSpace(2);
	*ptr++ = RPN_SIZEOF_SECTTYPE;
	*ptr = type;
}

void Expression::makeStartOfSectionType(SectionType type) {
	clear();
	data = "Section type's start is not known";

	uint8_t *ptr = reserveSpace(2);
	*ptr++ = RPN_STARTOF_SECTTYPE;
	*ptr = type;
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
	if (!sym || !sym->getSection() || !sym->isDefined())
		return false;

	assume(sym->isNumeric());

	Section const &sect = *sym->getSection();
	int32_t unknownBits = (1 << 16) - (1 << sect.align);

	// `sym->getValue()` attempts to add the section's address, but that's "-1"
	// because the section is floating (otherwise we wouldn't be here)
	assume(sect.org == (uint32_t)-1);
	int32_t symbolOfs = sym->getValue() + 1;

	int32_t knownBits = (symbolOfs + sect.alignOfs) & ~unknownBits;
	return knownBits != 0;
}

/*
 * Attempts to compute a constant LOW() from non-constant argument
 * This is possible if the argument is a symbol belonging to an `ALIGN[8]` section.
 *
 * @return The constant `LOW(expr)` result if it can be computed, or -1 otherwise.
 */
static int32_t tryConstLow(Expression const &expr) {
	Symbol const *sym = expr.symbolOf();
	if (!sym || !sym->getSection() || !sym->isDefined())
		return -1;

	assume(sym->isNumeric());

	// The low byte must not cover any unknown bits
	Section const &sect = *sym->getSection();
	if (sect.align < 8)
		return -1;

	// `sym->getValue()` attempts to add the section's address, but that's "-1"
	// because the section is floating (otherwise we wouldn't be here)
	assume(sect.org == (uint32_t)-1);
	int32_t symbolOfs = sym->getValue() + 1;

	return (symbolOfs + sect.alignOfs) & 0xFF;
}

/*
 * Attempts to compute a constant binary AND with one non-constant operands
 * This is possible if one operand is a symbol belonging to an `ALIGN[N]` section, and the other is
 * a constant that only keeps (some of) the lower N bits.
 *
 * @return The constant `lhs & rhs` result if it can be computed, or -1 otherwise.
 */
static int32_t tryConstMask(Expression const &lhs, Expression const &rhs) {
	Symbol const *lhsSymbol = lhs.symbolOf();
	Symbol const *rhsSymbol = lhsSymbol ? nullptr : rhs.symbolOf();
	bool lhsIsSymbol = lhsSymbol && lhsSymbol->getSection();
	bool rhsIsSymbol = rhsSymbol && rhsSymbol->getSection();

	if (!lhsIsSymbol && !rhsIsSymbol)
		return -1;

	// If the lhs isn't a symbol, try again the other way around
	Symbol const &sym = lhsIsSymbol ? *lhsSymbol : *rhsSymbol;
	Expression const &expr = lhsIsSymbol ? rhs : lhs; // Opposite side of `sym`

	if (!sym.isDefined() || !expr.isKnown())
		return -1;

	assume(sym.isNumeric());

	// We can now safely use `expr.value()`
	int32_t mask = expr.value();

	// The mask must not cover any unknown bits
	Section const &sect = *sym.getSection();
	if (int32_t unknownBits = (1 << 16) - (1 << sect.align); (unknownBits & mask) != 0)
		return -1;

	// `sym.getValue()` attempts to add the section's address, but that's "-1"
	// because the section is floating (otherwise we wouldn't be here)
	assume(sect.org == (uint32_t)-1);
	int32_t symbolOfs = sym.getValue() + 1;

	return (symbolOfs + sect.alignOfs) & mask;
}

void Expression::makeUnaryOp(RPNCommand op, Expression &&src) {
	clear();
	// First, check if the expression is known
	if (src.isKnown()) {
		// If the expressions is known, just compute the value
		int32_t val = src.value();

		switch (op) {
		case RPN_NEG:
			data = (int32_t) - (uint32_t)val;
			break;
		case RPN_NOT:
			data = ~val;
			break;
		case RPN_LOGNOT:
			data = !val;
			break;
		case RPN_HIGH:
			data = (int32_t)((uint32_t)val >> 8 & 0xFF);
			break;
		case RPN_LOW:
			data = val & 0xFF;
			break;
		case RPN_BITWIDTH:
			data = val != 0 ? 32 - clz((uint32_t)val) : 0;
			break;
		case RPN_TZCOUNT:
			data = val != 0 ? ctz((uint32_t)val) : 32;
			break;

		case RPN_LOGOR:
		case RPN_LOGAND:
		case RPN_LOGEQ:
		case RPN_LOGGT:
		case RPN_LOGLT:
		case RPN_LOGGE:
		case RPN_LOGLE:
		case RPN_LOGNE:
		case RPN_ADD:
		case RPN_SUB:
		case RPN_XOR:
		case RPN_OR:
		case RPN_AND:
		case RPN_SHL:
		case RPN_SHR:
		case RPN_USHR:
		case RPN_MUL:
		case RPN_DIV:
		case RPN_MOD:
		case RPN_EXP:
		case RPN_BANK_SYM:
		case RPN_BANK_SECT:
		case RPN_BANK_SELF:
		case RPN_SIZEOF_SECT:
		case RPN_STARTOF_SECT:
		case RPN_SIZEOF_SECTTYPE:
		case RPN_STARTOF_SECTTYPE:
		case RPN_HRAM:
		case RPN_RST:
		case RPN_CONST:
		case RPN_SYM:
			fatalerror("%d is not an unary operator\n", op);
		}
	} else if (op == RPN_LOGNOT && tryConstLogNot(src)) {
		data = 0;
	} else if (int32_t constVal; op == RPN_LOW && (constVal = tryConstLow(src)) != -1) {
		data = constVal;
	} else {
		// If it's not known, just reuse its RPN buffer and append the operator
		rpnPatchSize = src.rpnPatchSize;
		std::swap(rpn, src.rpn);
		data = std::move(src.data);
		*reserveSpace(1) = op;
	}
}

void Expression::makeBinaryOp(RPNCommand op, Expression &&src1, Expression const &src2) {
	clear();
	// First, check if the expressions are known
	if (src1.isKnown() && src2.isKnown()) {
		// If both expressions are known, just compute the value
		int32_t lval = src1.value(), rval = src2.value();

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
			data = (int32_t)((uint32_t)lval + (uint32_t)rval);
			break;
		case RPN_SUB:
			data = (int32_t)((uint32_t)lval - (uint32_t)rval);
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
			if (rval < 0)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting left by negative amount %" PRId32 "\n", rval
				);

			if (rval >= 32)
				warning(WARNING_SHIFT_AMOUNT, "Shifting left by large amount %" PRId32 "\n", rval);

			data = op_shift_left(lval, rval);
			break;
		case RPN_SHR:
			if (lval < 0)
				warning(WARNING_SHIFT, "Shifting right negative value %" PRId32 "\n", lval);

			if (rval < 0)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32 "\n", rval
				);

			if (rval >= 32)
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32 "\n", rval);

			data = op_shift_right(lval, rval);
			break;
		case RPN_USHR:
			if (rval < 0)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting right by negative amount %" PRId32 "\n", rval
				);

			if (rval >= 32)
				warning(WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32 "\n", rval);

			data = op_shift_right_unsigned(lval, rval);
			break;
		case RPN_MUL:
			data = (int32_t)((uint32_t)lval * (uint32_t)rval);
			break;
		case RPN_DIV:
			if (rval == 0)
				fatalerror("Division by zero\n");

			if (lval == INT32_MIN && rval == -1) {
				warning(
				    WARNING_DIV,
				    "Division of %" PRId32 " by -1 yields %" PRId32 "\n",
				    INT32_MIN,
				    INT32_MIN
				);
				data = INT32_MIN;
			} else {
				data = op_divide(lval, rval);
			}
			break;
		case RPN_MOD:
			if (rval == 0)
				fatalerror("Modulo by zero\n");

			if (lval == INT32_MIN && rval == -1)
				data = 0;
			else
				data = op_modulo(lval, rval);
			break;
		case RPN_EXP:
			if (rval < 0)
				fatalerror("Exponentiation by negative power\n");

			data = op_exponent(lval, rval);
			break;

		case RPN_NEG:
		case RPN_NOT:
		case RPN_LOGNOT:
		case RPN_BANK_SYM:
		case RPN_BANK_SECT:
		case RPN_BANK_SELF:
		case RPN_SIZEOF_SECT:
		case RPN_STARTOF_SECT:
		case RPN_SIZEOF_SECTTYPE:
		case RPN_STARTOF_SECTTYPE:
		case RPN_HRAM:
		case RPN_RST:
		case RPN_HIGH:
		case RPN_LOW:
		case RPN_BITWIDTH:
		case RPN_TZCOUNT:
		case RPN_CONST:
		case RPN_SYM:
			fatalerror("%d is not a binary operator\n", op);
		}
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
			uint8_t bytes[] = {
			    RPN_CONST,
			    (uint8_t)lval,
			    (uint8_t)(lval >> 8),
			    (uint8_t)(lval >> 16),
			    (uint8_t)(lval >> 24),
			};
			rpn.clear();
			rpnPatchSize = 0;
			memcpy(reserveSpace(sizeof(bytes)), bytes, sizeof(bytes));

			// Use the other expression's un-const reason
			data = std::move(src2.data);
		} else {
			// Otherwise just reuse its RPN buffer
			rpnPatchSize = src1.rpnPatchSize;
			std::swap(rpn, src1.rpn);
			data = std::move(src1.data);
		}

		// Now, merge the right expression into the left one
		if (src2.isKnown()) {
			// If the right expression is constant, append a shim instead
			uint32_t rval = src2.value();
			uint8_t bytes[] = {
			    RPN_CONST,
			    (uint8_t)rval,
			    (uint8_t)(rval >> 8),
			    (uint8_t)(rval >> 16),
			    (uint8_t)(rval >> 24),
			};
			uint8_t *ptr = reserveSpace(sizeof(bytes) + 1, sizeof(bytes) + 1);
			memcpy(ptr, bytes, sizeof(bytes));
			ptr[sizeof(bytes)] = op;
		} else {
			// Copy the right RPN and append the operator
			uint32_t rightRpnSize = src2.rpn.size();
			uint8_t *ptr = reserveSpace(rightRpnSize + 1, src2.rpnPatchSize + 1);
			if (rightRpnSize > 0)
				// If `rightRpnSize == 0`, then `memcpy(ptr, nullptr, rightRpnSize)` would be UB
				memcpy(ptr, src2.rpn.data(), rightRpnSize);
			ptr[rightRpnSize] = op;
		}
	}
}

void Expression::makeCheckHRAM() {
	isSymbol = false;
	if (!isKnown()) {
		*reserveSpace(1) = RPN_HRAM;
	} else if (int32_t val = value(); val >= 0xFF00 && val <= 0xFFFF) {
		// That range is valid, but only keep the lower byte
		data = val & 0xFF;
	} else if (val < 0 || val > 0xFF) {
		error("Source address $%" PRIx32 " not between $FF00 to $FFFF\n", val);
	}
}

void Expression::makeCheckRST() {
	if (!isKnown()) {
		*reserveSpace(1) = RPN_RST;
	} else if (int32_t val = value(); val & ~0x38) {
		// A valid RST address must be masked with 0x38
		error("Invalid address $%" PRIx32 " for RST\n", val);
	} else {
		// The target is in the "0x38" bits, all other bits are set
		data = val | 0xC7;
	}
}

// Checks that an RPN expression's value fits within N bits (signed or unsigned)
void Expression::checkNBit(uint8_t n) const {
	if (isKnown())
		::checkNBit(value(), n, "Expression");
}

bool checkNBit(int32_t v, uint8_t n, char const *name) {
	assume(n != 0);                     // That doesn't make sense
	assume(n < CHAR_BIT * sizeof(int)); // Otherwise `1 << n` is UB

	if (v < -(1 << n) || v >= 1 << n) {
		warning(WARNING_TRUNCATION_1, "%s must be %u-bit\n", name, n);
		return false;
	}
	if (v < -(1 << (n - 1))) {
		warning(WARNING_TRUNCATION_2, "%s must be %u-bit\n", name, n);
		return false;
	}

	return true;
}
