/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_SECTION_H
#define RGBDS_SECTION_H

#include <deque>
#include <stdint.h>
#include <vector>

#include "linkdefs.hpp"
#include "platform.hpp" // NONNULL

extern uint8_t fillByte;

struct Expression;
struct FileStackNode;
struct Section;

struct Patch {
	FileStackNode const *src;
	uint32_t lineNo;
	uint32_t offset;
	Section *pcSection;
	uint32_t pcOffset;
	uint8_t type;
	std::vector<uint8_t> rpn;
};

struct Section {
	char *name;
	enum SectionType type;
	enum SectionModifier modifier;
	FileStackNode const *src; // Where the section was defined
	uint32_t fileLine; // Line where the section was defined
	uint32_t size;
	uint32_t org;
	uint32_t bank;
	uint8_t align; // Exactly as specified in `ALIGN[]`
	uint16_t alignOfs;
	std::deque<Patch> patches;
	std::vector<uint8_t> data;
};

struct SectionSpec {
	uint32_t bank;
	uint8_t alignment;
	uint16_t alignOfs;
};

extern std::deque<Section> sectionList;
extern Section *currentSection;

Section *sect_FindSectionByName(char const *name);
void sect_NewSection(char const *name, enum SectionType type, uint32_t org,
		     SectionSpec const *attributes, enum SectionModifier mod);
void sect_SetLoadSection(char const *name, enum SectionType type, uint32_t org,
			 SectionSpec const *attributes, enum SectionModifier mod);
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
void sect_AbsByteGroup(uint8_t const *s, size_t length);
void sect_AbsWordGroup(uint8_t const *s, size_t length);
void sect_AbsLongGroup(uint8_t const *s, size_t length);
void sect_Skip(uint32_t skip, bool ds);
void sect_RelByte(Expression *expr, uint32_t pcShift);
void sect_RelBytes(uint32_t n, std::vector<Expression> &exprs);
void sect_RelWord(Expression *expr, uint32_t pcShift);
void sect_RelLong(Expression *expr, uint32_t pcShift);
void sect_PCRelByte(Expression *expr, uint32_t pcShift);
void sect_BinaryFile(char const *s, int32_t startPos);
void sect_BinaryFileSlice(char const *s, int32_t start_pos, int32_t length);

void sect_EndSection();
void sect_PushSection();
void sect_PopSection();

bool sect_IsSizeKnown(Section const NONNULL(name));

#endif // RGBDS_SECTION_H
