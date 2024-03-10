/* SPDX-License-Identifier: MIT */

#include "asm/rpn.hpp"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "opmath.hpp"

#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/section.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

// Init a RPN expression
static void initExpression(Expression &expr) {
	expr.val = 0;
	expr.reason.clear();
	expr.isKnown = true;
	expr.isSymbol = false;
	expr.rpn.clear();
	expr.rpnPatchSize = 0;
}

// Makes an expression "not known", also setting its error message
template<typename... Ts>
static void makeUnknown(Expression &expr, Ts... parts) {
	expr.isKnown = false;
	expr.reason.clear();
	(expr.reason.append(parts), ...);
}

static uint8_t *reserveSpace(Expression &expr, uint32_t size) {
	size_t curSize = expr.rpn.size();

	expr.rpn.resize(curSize + size);
	return &expr.rpn[curSize];
}

// Add symbols, constants and operators to expression
void rpn_Number(Expression &expr, uint32_t val) {
	initExpression(expr);
	expr.val = val;
}

void rpn_Symbol(Expression &expr, char const *symName) {
	initExpression(expr);

	Symbol *sym = sym_FindScopedSymbol(symName);

	if (sym_IsPC(sym) && !sect_GetSymbolSection()) {
		error("PC has no value outside a section\n");
		expr.val = 0;
	} else if (!sym || !sym->isConstant()) {
		expr.isSymbol = true;

		if (sym_IsPC(sym))
			makeUnknown(expr, "PC is not constant at assembly time");
		else
			makeUnknown(expr, "'", symName, "' is not constant at assembly time");
		sym = sym_Ref(symName);
		expr.rpnPatchSize += 5; // 1-byte opcode + 4-byte symbol ID

		size_t nameLen = sym->name.length() + 1; // Don't forget NUL!
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);
		*ptr++ = RPN_SYM;
		memcpy(ptr, sym->name.c_str(), nameLen);
	} else {
		expr.val = sym_GetConstantValue(symName);
	}
}

static void bankSelf(Expression &expr) {
	initExpression(expr);

	if (!currentSection) {
		error("PC has no bank outside a section\n");
		expr.val = 1;
	} else if (currentSection->bank == (uint32_t)-1) {
		makeUnknown(expr, "Current section's bank is not known");
		expr.rpnPatchSize++;
		*reserveSpace(expr, 1) = RPN_BANK_SELF;
	} else {
		expr.val = currentSection->bank;
	}
}

void rpn_BankSymbol(Expression &expr, char const *symName) {
	Symbol const *sym = sym_FindScopedSymbol(symName);

	// The @ symbol is treated differently.
	if (sym_IsPC(sym)) {
		bankSelf(expr);
		return;
	}

	initExpression(expr);
	if (sym && !sym->isLabel()) {
		error("BANK argument must be a label\n");
		expr.val = 1;
	} else {
		sym = sym_Ref(symName);
		assert(sym); // If the symbol didn't exist, it should have been created

		if (sym->getSection() && sym->getSection()->bank != (uint32_t)-1) {
			// Symbol's section is known and bank is fixed
			expr.val = sym->getSection()->bank;
		} else {
			makeUnknown(expr, "\"", symName, "\"'s bank is not known");
			expr.rpnPatchSize += 5; // opcode + 4-byte sect ID

			size_t nameLen = sym->name.length() + 1; // Room for NUL!
			uint8_t *ptr = reserveSpace(expr, nameLen + 1);

			*ptr++ = RPN_BANK_SYM;
			memcpy(ptr, sym->name.c_str(), nameLen);
		}
	}
}

void rpn_BankSection(Expression &expr, char const *sectionName) {
	initExpression(expr);

	Section *section = sect_FindSectionByName(sectionName);

	if (section && section->bank != (uint32_t)-1) {
		expr.val = section->bank;
	} else {
		makeUnknown(expr, "Section \"", sectionName, "\"'s bank is not known");

		size_t nameLen = strlen(sectionName) + 1; // Room for NUL!
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);

		expr.rpnPatchSize += nameLen + 1;
		*ptr++ = RPN_BANK_SECT;
		memcpy(ptr, sectionName, nameLen);
	}
}

void rpn_SizeOfSection(Expression &expr, char const *sectionName) {
	initExpression(expr);

	Section *section = sect_FindSectionByName(sectionName);

	if (section && section->isSizeKnown()) {
		expr.val = section->size;
	} else {
		makeUnknown(expr, "Section \"", sectionName, "\"'s size is not known");

		size_t nameLen = strlen(sectionName) + 1; // Room for NUL!
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);

		expr.rpnPatchSize += nameLen + 1;
		*ptr++ = RPN_SIZEOF_SECT;
		memcpy(ptr, sectionName, nameLen);
	}
}

void rpn_StartOfSection(Expression &expr, char const *sectionName) {
	initExpression(expr);

	Section *section = sect_FindSectionByName(sectionName);

	if (section && section->org != (uint32_t)-1) {
		expr.val = section->org;
	} else {
		makeUnknown(expr, "Section \"", sectionName, "\"'s start is not known");

		size_t nameLen = strlen(sectionName) + 1; // Room for NUL!
		uint8_t *ptr = reserveSpace(expr, nameLen + 1);

		expr.rpnPatchSize += nameLen + 1;
		*ptr++ = RPN_STARTOF_SECT;
		memcpy(ptr, sectionName, nameLen);
	}
}

void rpn_SizeOfSectionType(Expression &expr, SectionType type) {
	initExpression(expr);
	makeUnknown(expr, "Section type's size is not known");

	uint8_t *ptr = reserveSpace(expr, 2);

	expr.rpnPatchSize += 2;
	*ptr++ = RPN_SIZEOF_SECTTYPE;
	*ptr++ = type;
}

void rpn_StartOfSectionType(Expression &expr, SectionType type) {
	initExpression(expr);
	makeUnknown(expr, "Section type's start is not known");

	uint8_t *ptr = reserveSpace(expr, 2);

	expr.rpnPatchSize += 2;
	*ptr++ = RPN_STARTOF_SECTTYPE;
	*ptr++ = type;
}

void rpn_CheckHRAM(Expression &expr) {
	expr.isSymbol = false;

	if (!expr.isKnown) {
		expr.rpnPatchSize++;
		*reserveSpace(expr, 1) = RPN_HRAM;
	} else if (expr.val >= 0xFF00 && expr.val <= 0xFFFF) {
		// That range is valid, but only keep the lower byte
		expr.val &= 0xFF;
	} else if (expr.val < 0 || expr.val > 0xFF) {
		error("Source address $%" PRIx32 " not between $FF00 to $FFFF\n", expr.val);
	}
}

void rpn_CheckRST(Expression &expr) {
	if (expr.isKnown) {
		// A valid RST address must be masked with 0x38
		if (expr.val & ~0x38)
			error("Invalid address $%" PRIx32 " for RST\n", expr.val);
		// The target is in the "0x38" bits, all other bits are set
		expr.val |= 0xC7;
	} else {
		expr.rpnPatchSize++;
		*reserveSpace(expr, 1) = RPN_RST;
	}
}

// Checks that an RPN expression's value fits within N bits (signed or unsigned)
void rpn_CheckNBit(const Expression &expr, uint8_t n) {
	assert(n != 0);                     // That doesn't make sense
	assert(n < CHAR_BIT * sizeof(int)); // Otherwise `1 << n` is UB

	if (expr.isKnown) {
		int32_t val = expr.val;

		if (val < -(1 << n) || val >= 1 << n)
			warning(WARNING_TRUNCATION_1, "Expression must be %u-bit\n", n);
		else if (val < -(1 << (n - 1)))
			warning(WARNING_TRUNCATION_2, "Expression must be %u-bit\n", n);
	}
}

int32_t Expression::getConstVal() const {
	if (!isKnown) {
		error("Expected constant expression: %s\n", reason.c_str());
		return 0;
	}
	return val;
}

void rpn_LOGNOT(Expression &expr, Expression &&src) {
	expr = std::move(src);
	expr.isSymbol = false;

	if (expr.isKnown) {
		expr.val = !expr.val;
	} else {
		expr.rpnPatchSize++;
		*reserveSpace(expr, 1) = RPN_LOGNOT;
	}
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

	Section const *section1 = sym1->getSection();
	Section const *section2 = sym->getSection();
	return section1 && (section1 == section2);
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

void rpn_BinaryOp(RPNCommand op, Expression &expr, Expression &&src1, const Expression &src2) {
	initExpression(expr);
	expr.isSymbol = false;
	int32_t constMaskVal;

	// First, check if the expression is known
	expr.isKnown = src1.isKnown && src2.isKnown;
	if (expr.isKnown) {
		// If both expressions are known, just compute the value
		uint32_t uleft = src1.val, uright = src2.val;

		switch (op) {
		case RPN_LOGOR:
			expr.val = src1.val || src2.val;
			break;
		case RPN_LOGAND:
			expr.val = src1.val && src2.val;
			break;
		case RPN_LOGEQ:
			expr.val = src1.val == src2.val;
			break;
		case RPN_LOGGT:
			expr.val = src1.val > src2.val;
			break;
		case RPN_LOGLT:
			expr.val = src1.val < src2.val;
			break;
		case RPN_LOGGE:
			expr.val = src1.val >= src2.val;
			break;
		case RPN_LOGLE:
			expr.val = src1.val <= src2.val;
			break;
		case RPN_LOGNE:
			expr.val = src1.val != src2.val;
			break;
		case RPN_ADD:
			expr.val = uleft + uright;
			break;
		case RPN_SUB:
			expr.val = uleft - uright;
			break;
		case RPN_XOR:
			expr.val = src1.val ^ src2.val;
			break;
		case RPN_OR:
			expr.val = src1.val | src2.val;
			break;
		case RPN_AND:
			expr.val = src1.val & src2.val;
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

			expr.val = op_shift_left(src1.val, src2.val);
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

			expr.val = op_shift_right(src1.val, src2.val);
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

			expr.val = op_shift_right_unsigned(src1.val, src2.val);
			break;
		case RPN_MUL:
			expr.val = uleft * uright;
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
				expr.val = INT32_MIN;
			} else {
				expr.val = op_divide(src1.val, src2.val);
			}
			break;
		case RPN_MOD:
			if (src2.val == 0)
				fatalerror("Modulo by zero\n");

			if (src1.val == INT32_MIN && src2.val == -1)
				expr.val = 0;
			else
				expr.val = op_modulo(src1.val, src2.val);
			break;
		case RPN_EXP:
			if (src2.val < 0)
				fatalerror("Exponentiation by negative power\n");

			expr.val = op_exponent(src1.val, src2.val);
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

		expr.val = symbol1.getValue() - symbol2.getValue();
		expr.isKnown = true;
	} else if (op == RPN_AND && (constMaskVal = tryConstMask(src1, src2)) != -1) {
		expr.val = constMaskVal;
		expr.isKnown = true;
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
			expr.rpnPatchSize = sizeof(bytes);
			expr.rpn.clear();
			memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));

			// Use the other expression's un-const reason
			expr.reason = src2.reason;
		} else {
			// Otherwise just reuse its RPN buffer
			expr.rpnPatchSize = src1.rpnPatchSize;
			std::swap(expr.rpn, src1.rpn);
			expr.reason = src1.reason;
		}

		// Now, merge the right expression into the left one
		uint8_t const *ptr = nullptr;
		uint32_t len = 0;
		uint32_t patchSize = 0;

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
			ptr = bytes;
			len = sizeof(bytes);
			patchSize = sizeof(bytes);
		} else {
			ptr = src2.rpn.data(); // Pointer to the right RPN
			len = src2.rpn.size(); // Size of the right RPN
			patchSize = src2.rpnPatchSize;
		}
		// Copy the right RPN and append the operator
		uint8_t *buf = reserveSpace(expr, len + 1);

		if (ptr)
			// If there was none, `memcpy(buf, nullptr, 0)` would be UB
			memcpy(buf, ptr, len);
		buf[len] = op;

		expr.rpnPatchSize += patchSize + 1;
	}
}

void rpn_HIGH(Expression &expr, Expression &&src) {
	expr = std::move(src);
	expr.isSymbol = false;

	if (expr.isKnown) {
		expr.val = (uint32_t)expr.val >> 8 & 0xFF;
	} else {
		uint8_t bytes[] = {RPN_CONST, 8, 0, 0, 0, RPN_SHR, RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};
		expr.rpnPatchSize += sizeof(bytes);
		memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
	}
}

void rpn_LOW(Expression &expr, Expression &&src) {
	expr = std::move(src);
	expr.isSymbol = false;

	if (expr.isKnown) {
		expr.val = expr.val & 0xFF;
	} else {
		uint8_t bytes[] = {RPN_CONST, 0xFF, 0, 0, 0, RPN_AND};

		expr.rpnPatchSize += sizeof(bytes);
		memcpy(reserveSpace(expr, sizeof(bytes)), bytes, sizeof(bytes));
	}
}

void rpn_ISCONST(Expression &expr, const Expression &src) {
	initExpression(expr);
	expr.val = src.isKnown;
	expr.isKnown = true;
	expr.isSymbol = false;
}

void rpn_NEG(Expression &expr, Expression &&src) {
	expr = std::move(src);
	expr.isSymbol = false;

	if (expr.isKnown) {
		expr.val = -(uint32_t)expr.val;
	} else {
		expr.rpnPatchSize++;
		*reserveSpace(expr, 1) = RPN_NEG;
	}
}

void rpn_NOT(Expression &expr, Expression &&src) {
	expr = std::move(src);
	expr.isSymbol = false;

	if (expr.isKnown) {
		expr.val = ~expr.val;
	} else {
		expr.rpnPatchSize++;
		*reserveSpace(expr, 1) = RPN_NOT;
	}
}
