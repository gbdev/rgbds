/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_SECTION_HPP
#define RGBDS_ASM_SECTION_HPP

#include <deque>
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "linkdefs.hpp"

extern uint8_t fillByte;

struct Expression;
struct FileStackNode;
struct Section;

struct Patch {
	std::shared_ptr<FileStackNode> src;
	uint32_t lineNo;
	uint32_t offset;
	Section *pcSection;
	uint32_t pcOffset;
	uint8_t type;
	std::vector<uint8_t> rpn;
};

struct Section {
	std::string name;
	SectionType type;
	SectionModifier modifier;
	std::shared_ptr<FileStackNode> src; // Where the section was defined
	uint32_t fileLine;                  // Line where the section was defined
	uint32_t size;
	uint32_t org;
	uint32_t bank;
	uint8_t align; // Exactly as specified in `ALIGN[]`
	uint16_t alignOfs;
	std::deque<Patch> patches;
	std::vector<uint8_t> data;

	bool isSizeKnown() const;
};

struct SectionSpec {
	uint32_t bank;
	uint8_t alignment;
	uint16_t alignOfs;
};

extern std::deque<Section> sectionList;
extern std::unordered_map<std::string, size_t> sectionMap; // Indexes into `sectionList`
extern Section *currentSection;

Section *sect_FindSectionByName(std::string const &name);
void sect_NewSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
);
void sect_SetLoadSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
);
void sect_EndLoadSection();

Section *sect_GetSymbolSection();
uint32_t sect_GetSymbolOffset();
uint32_t sect_GetOutputOffset();
uint32_t sect_GetAlignBytes(uint8_t alignment, uint16_t offset);
void sect_AlignPC(uint8_t alignment, uint16_t offset);

void sect_StartUnion();
void sect_NextUnionMember();
void sect_EndUnion();
void sect_CheckUnionClosed();

void sect_AbsByte(uint8_t b);
void sect_AbsByteString(std::vector<int32_t> const &s);
void sect_AbsWordString(std::vector<int32_t> const &s);
void sect_AbsLongString(std::vector<int32_t> const &s);
void sect_Skip(uint32_t skip, bool ds);
void sect_RelByte(Expression &expr, uint32_t pcShift);
void sect_RelBytes(uint32_t n, std::vector<Expression> &exprs);
void sect_RelWord(Expression &expr, uint32_t pcShift);
void sect_RelLong(Expression &expr, uint32_t pcShift);
void sect_PCRelByte(Expression &expr, uint32_t pcShift);
void sect_BinaryFile(std::string const &name, int32_t startPos);
void sect_BinaryFileSlice(std::string const &name, int32_t startPos, int32_t length);

void sect_EndSection();
void sect_PushSection();
void sect_PopSection();

#endif // RGBDS_ASM_SECTION_HPP
