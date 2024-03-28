/* SPDX-License-Identifier: MIT */

#include "asm/rpn.hpp"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>

#include "opmath.hpp"

#include "asm/output.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

void Expression::clear() {
	val = 0;
	reason.clear();
	isKnown = true;
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
	if (!isKnown) {
		error("Expected constant expression: %s\n", reason.c_str());
		return 0;
	}
	return val;
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
	val = value;
}

void Expression::makeSymbol(std::string const &symName) {
	clear();
	if (Symbol *sym = sym_FindScopedSymbol(symName); sym_IsPC(sym) && !sect_GetSymbolSection()) {
		error("PC has no value outside a section\n");
		val = 0;
	} else if (!sym || !sym->isConstant()) {
		isSymbol = true;

		if (sym_IsPC(sym))
			makeUnknown("PC is not constant at assembly time");
		else
			makeUnknown("'", symName, "' is not constant at assembly time");
		sym = sym_Ref(symName);

		size_t nameLen = sym->name.length() + 1; // Don't forget NUL!

		// 1-byte opcode + 4-byte symbol ID
		uint8_t *ptr = reserveSpace(nameLen + 1, 5);
		*ptr++ = RPN_SYM;
		memcpy(ptr, sym->name.c_str(), nameLen);
	} else {
		val = sym_GetConstantValue(symName);
	}
}

void Expression::makeBankSymbol(std::string const &symName) {
	clear();
	if (Symbol const *sym = sym_FindScopedSymbol(symName); sym_IsPC(sym)) {
		// The @ symbol is treated differently.
		if (!currentSection) {
			error("PC has no bank outside a section\n");
			val = 1;
		} else if (currentSection->bank == (uint32_t)-1) {
			makeUnknown("Current section's bank is not known");

			*reserveSpace(1) = RPN_BANK_SELF;
		} else {
			val = currentSection->bank;
		}
		return;
	} else if (sym && !sym->isLabel()) {
		error("BANK argument must be a label\n");
		val = 1;
	} else {
		sym = sym_Ref(symName);
		assert(sym); // If the symbol didn't exist, it should have been created

		if (sym->getSection() && sym->getSection()->bank != (uint32_t)-1) {
			// Symbol's section is known and bank is fixed
			val = sym->getSection()->bank;
		} else {
			makeUnknown("\"", symName, "\"'s bank is not known");

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
		val = sect->bank;
	} else {
		makeUnknown("Section \"", sectName, "\"'s bank is not known");

		size_t nameLen = sectName.length() + 1; // Room for NUL!

		uint8_t *ptr = reserveSpace(nameLen + 1);
		*ptr++ = RPN_BANK_SECT;
		memcpy(ptr, sectName.data(), nameLen);
	}
}

void Expression::makeSizeOfSection(std::string const &sectName) {
	clear();
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->isSizeKnown()) {
		val = sect->size;
	} else {
		makeUnknown("Section \"", sectName, "\"'s size is not known");

		size_t nameLen = sectName.length() + 1; // Room for NUL!

		uint8_t *ptr = reserveSpace(nameLen + 1);
		*ptr++ = RPN_SIZEOF_SECT;
		memcpy(ptr, sectName.data(), nameLen);
	}
}

void Expression::makeStartOfSection(std::string const &sectName) {
	clear();
	if (Section *sect = sect_FindSectionByName(sectName); sect && sect->org != (uint32_t)-1) {
		val = sect->org;
	} else {
		makeUnknown("Section \"", sectName, "\"'s start is not known");

		size_t nameLen = sectName.length() + 1; // Room for NUL!

		uint8_t *ptr = reserveSpace(nameLen + 1);
		*ptr++ = RPN_STARTOF_SECT;
		memcpy(ptr, sectName.data(), nameLen);
	}
}

void Expression::makeSizeOfSectionType(SectionType type) {
	clear();
	makeUnknown("Section type's size is not known");

	uint8_t *ptr = reserveSpace(2);
	*ptr++ = RPN_SIZEOF_SECTTYPE;
	*ptr = type;
}

void Expression::makeStartOfSectionType(SectionType type) {
	clear();
	makeUnknown("Section type's start is not known");

	uint8_t *ptr = reserveSpace(2);
	*ptr++ = RPN_STARTOF_SECTTYPE;
	*ptr = type;
}

/*
 * Attempts to compute a constant binary AND from non-constant operands
 * This is possible if one operand is a symbol belonging to an `ALIGN[N]` section, and the other is
 * a constant that only keeps (some of) the lower N bits.
 *
 * @return The constant result if it can be computed, or -1 otherwise.
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

	assert(sym.isNumeric());

	if (!expr.isKnown)
		return -1;
	// We can now safely use `expr.val`
	Section const &sect = *sym.getSection();
	int32_t unknownBits = (1 << 16) - (1 << sect.align); // The max alignment is 16

	// The mask must ignore all unknown bits
	if ((expr.val & unknownBits) != 0)
		return -1;

	// `sym.getValue()` attempts to add the section's address, but that's "-1"
	// because the section is floating (otherwise we wouldn't be here)
	assert(sect.org == (uint32_t)-1);
	int32_t symbolOfs = sym.getValue() + 1;

	return (symbolOfs + sect.alignOfs) & ~unknownBits;
}

void Expression::makeHigh() {
	isSymbol = false;
	if (isKnown) {
		val = (uint32_t)val >> 8 & 0xFF;
	} else {
		uint8_t bytes[] = {RPN_CONST, 8, 0, 0, 0, RPN_SHR, RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

		memcpy(reserveSpace(sizeof(bytes)), bytes, sizeof(bytes));
	}
}

void Expression::makeLow() {
	isSymbol = false;
	if (isKnown) {
		val = val & 0xFF;
	} else {
		uint8_t bytes[] = {RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

		memcpy(reserveSpace(sizeof(bytes)), bytes, sizeof(bytes));
	}
}

void Expression::makeNeg() {
	isSymbol = false;
	if (isKnown) {
		val = -(uint32_t)val;
	} else {
		*reserveSpace(1) = RPN_NEG;
	}
}

void Expression::makeNot() {
	isSymbol = false;
	if (isKnown) {
		val = ~val;
	} else {
		*reserveSpace(1) = RPN_NOT;
	}
}

void Expression::makeLogicNot() {
	isSymbol = false;
	if (isKnown) {
		val = !val;
	} else {
		*reserveSpace(1) = RPN_LOGNOT;
	}
}

void Expression::makeBinaryOp(RPNCommand op, Expression &&src1, Expression const &src2) {
	clear();
	// First, check if the expression is known
	isKnown = src1.isKnown && src2.isKnown;
	if (isKnown) {
		// If both expressions are known, just compute the value
		uint32_t uleft = src1.val, uright = src2.val;

		switch (op) {
		case RPN_LOGOR:
			val = src1.val || src2.val;
			break;
		case RPN_LOGAND:
			val = src1.val && src2.val;
			break;
		case RPN_LOGEQ:
			val = src1.val == src2.val;
			break;
		case RPN_LOGGT:
			val = src1.val > src2.val;
			break;
		case RPN_LOGLT:
			val = src1.val < src2.val;
			break;
		case RPN_LOGGE:
			val = src1.val >= src2.val;
			break;
		case RPN_LOGLE:
			val = src1.val <= src2.val;
			break;
		case RPN_LOGNE:
			val = src1.val != src2.val;
			break;
		case RPN_ADD:
			val = uleft + uright;
			break;
		case RPN_SUB:
			val = uleft - uright;
			break;
		case RPN_XOR:
			val = src1.val ^ src2.val;
			break;
		case RPN_OR:
			val = src1.val | src2.val;
			break;
		case RPN_AND:
			val = src1.val & src2.val;
			break;
		case RPN_SHL:
			if (src2.val < 0)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting left by negative amount %" PRId32 "\n", src2.val
				);

			if (src2.val >= 32)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting left by large amount %" PRId32 "\n", src2.val
				);

			val = op_shift_left(src1.val, src2.val);
			break;
		case RPN_SHR:
			if (src1.val < 0)
				warning(WARNING_SHIFT, "Shifting right negative value %" PRId32 "\n", src1.val);

			if (src2.val < 0)
				warning(
				    WARNING_SHIFT_AMOUNT,
				    "Shifting right by negative amount %" PRId32 "\n",
				    src2.val
				);

			if (src2.val >= 32)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32 "\n", src2.val
				);

			val = op_shift_right(src1.val, src2.val);
			break;
		case RPN_USHR:
			if (src2.val < 0)
				warning(
				    WARNING_SHIFT_AMOUNT,
				    "Shifting right by negative amount %" PRId32 "\n",
				    src2.val
				);

			if (src2.val >= 32)
				warning(
				    WARNING_SHIFT_AMOUNT, "Shifting right by large amount %" PRId32 "\n", src2.val
				);

			val = op_shift_right_unsigned(src1.val, src2.val);
			break;
		case RPN_MUL:
			val = uleft * uright;
			break;
		case RPN_DIV:
			if (src2.val == 0)
				fatalerror("Division by zero\n");

			if (src1.val == INT32_MIN && src2.val == -1) {
				warning(
				    WARNING_DIV,
				    "Division of %" PRId32 " by -1 yields %" PRId32 "\n",
				    INT32_MIN,
				    INT32_MIN
				);
				val = INT32_MIN;
			} else {
				val = op_divide(src1.val, src2.val);
			}
			break;
		case RPN_MOD:
			if (src2.val == 0)
				fatalerror("Modulo by zero\n");

			if (src1.val == INT32_MIN && src2.val == -1)
				val = 0;
			else
				val = op_modulo(src1.val, src2.val);
			break;
		case RPN_EXP:
			if (src2.val < 0)
				fatalerror("Exponentiation by negative power\n");

			val = op_exponent(src1.val, src2.val);
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
		case RPN_CONST:
		case RPN_SYM:
			fatalerror("%d is not a binary operator\n", op);
		}
	} else if (op == RPN_SUB && src1.isDiffConstant(src2.symbolOf())) {
		Symbol const &symbol1 = *src1.symbolOf();
		Symbol const &symbol2 = *src2.symbolOf();

		val = symbol1.getValue() - symbol2.getValue();
		isKnown = true;
	} else if (int32_t constVal; op == RPN_AND && (constVal = tryConstMask(src1, src2)) != -1) {
		val = constVal;
		isKnown = true;
	} else {
		// If it's not known, start computing the RPN expression

		// Convert the left-hand expression if it's constant
		if (src1.isKnown) {
			uint32_t lval = src1.val;
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
			reason = src2.reason;
		} else {
			// Otherwise just reuse its RPN buffer
			rpnPatchSize = src1.rpnPatchSize;
			std::swap(rpn, src1.rpn);
			reason = src1.reason;
		}

		// Now, merge the right expression into the left one
		uint8_t const *srcBytes = nullptr;
		uint32_t srcLen = 0;
		uint32_t srcPatchSize = 0;

		// If the right expression is constant, merge a shim instead
		uint32_t rval = src2.val;
		uint8_t bytes[] = {
		    RPN_CONST,
		    (uint8_t)rval,
		    (uint8_t)(rval >> 8),
		    (uint8_t)(rval >> 16),
		    (uint8_t)(rval >> 24),
		};
		if (src2.isKnown) {
			srcBytes = bytes;
			srcLen = sizeof(bytes);
			srcPatchSize = sizeof(bytes);
		} else {
			srcBytes = src2.rpn.data(); // Pointer to the right RPN
			srcLen = src2.rpn.size();   // Size of the right RPN
			srcPatchSize = src2.rpnPatchSize;
		}
		// Copy the right RPN and append the operator
		uint8_t *ptr = reserveSpace(srcLen + 1, srcPatchSize + 1);
		if (srcBytes)
			// If there were no `srcBytes`, then `memcpy(ptr, nullptr, 0)` would be UB
			memcpy(ptr, srcBytes, srcLen);
		ptr[srcLen] = op;
	}
}

void Expression::makeCheckHRAM() {
	isSymbol = false;
	if (!isKnown) {
		*reserveSpace(1) = RPN_HRAM;
	} else if (val >= 0xFF00 && val <= 0xFFFF) {
		// That range is valid, but only keep the lower byte
		val &= 0xFF;
	} else if (val < 0 || val > 0xFF) {
		error("Source address $%" PRIx32 " not between $FF00 to $FFFF\n", val);
	}
}

void Expression::makeCheckRST() {
	if (isKnown) {
		// A valid RST address must be masked with 0x38
		if (val & ~0x38)
			error("Invalid address $%" PRIx32 " for RST\n", val);
		// The target is in the "0x38" bits, all other bits are set
		val |= 0xC7;
	} else {
		*reserveSpace(1) = RPN_RST;
	}
}

// Checks that an RPN expression's value fits within N bits (signed or unsigned)
void Expression::checkNBit(uint8_t n) const {
	assert(n != 0);                     // That doesn't make sense
	assert(n < CHAR_BIT * sizeof(int)); // Otherwise `1 << n` is UB

	if (isKnown) {
		if (val < -(1 << n) || val >= 1 << n)
			warning(WARNING_TRUNCATION_1, "Expression must be %u-bit\n", n);
		else if (val < -(1 << (n - 1)))
			warning(WARNING_TRUNCATION_2, "Expression must be %u-bit\n", n);
	}
}
