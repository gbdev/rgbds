// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_RPN_HPP
#define RGBDS_ASM_RPN_HPP

#include <stdint.h>
#include <string>
#include <variant>
#include <vector>

#include "linkdefs.hpp"

struct Symbol;

struct RPNValue {
	RPNCommand command;
	std::variant<std::monostate, uint8_t, uint32_t, std::string> data;
};

struct Expression {
	std::variant<
	    int32_t,    // If the expression's value is known, it's here
	    std::string // Why the expression is not known, if it isn't
	    >
	    data = 0;
	std::vector<RPNValue> rpn{}; // Values to be serialized into the RPN expression

	bool isKnown() const { return std::holds_alternative<int32_t>(data); }
	int32_t value() const { return std::get<int32_t>(data); }

	int32_t getConstVal() const;
	Symbol const *symbolOf() const;
	bool isDiffConstant(Symbol const *symName) const;

	void makeNumber(uint32_t value);
	void makeSymbol(std::string const &symName);
	void makeBankSymbol(std::string const &symName);
	void makeBankSection(std::string const &sectName);
	void makeSizeOfSection(std::string const &sectName);
	void makeStartOfSection(std::string const &sectName);
	void makeSizeOfSectionType(SectionType type);
	void makeStartOfSectionType(SectionType type);
	void makeUnaryOp(RPNCommand op, Expression &&src);
	void makeBinaryOp(RPNCommand op, Expression &&src1, Expression const &src2);

	void makeCheckHRAM();
	void makeCheckRST();
	void makeCheckBitIndex(uint8_t mask);

	void checkNBit(uint8_t n) const;
};

bool checkNBit(int32_t v, uint8_t n, char const *name);

#endif // RGBDS_ASM_RPN_HPP
