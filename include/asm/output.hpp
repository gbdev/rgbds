// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_OUTPUT_HPP
#define RGBDS_ASM_OUTPUT_HPP

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include "linkdefs.hpp"

struct Expression;
struct FileStackNode;
struct Section;
struct Symbol;

struct Patch {
	Patch(
	    uint32_t type,
	    Expression const &expr,
	    uint32_t offset,
	    Section *pcSection,
	    uint32_t pcOffset
	);

	std::shared_ptr<FileStackNode> src;
	uint32_t lineNo;
	uint32_t offset;    // Offset in the output section
	Section *pcSection; // Symbol section, which can differ from the output section in LOAD blocks
	uint32_t pcOffset;  // Offset in pcSection at the start of the expression
	uint8_t type;
	std::vector<uint8_t> rpn;
};

enum StateFeature { STATE_EQU, STATE_VAR, STATE_EQUS, STATE_CHAR, STATE_MACRO, NB_STATE_FEATURES };

void out_RegisterNode(std::shared_ptr<FileStackNode> node);
void out_RegisterSymbol(Symbol &sym);
void out_CreateAssert(
    AssertionType type, Expression const &expr, std::string const &message, uint32_t ofs
);
void out_WriteObject();
void out_WriteState(std::string name, std::vector<StateFeature> const &features);

#endif // RGBDS_ASM_OUTPUT_HPP
