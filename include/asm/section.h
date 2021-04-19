/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_SECTION_H
#define RGBDS_SECTION_H

#include <stdint.h>
#include <stdbool.h>

#include "linkdefs.h"

extern uint8_t fillByte;

struct Expression;

struct Section {
	char *name;
	enum SectionType type;
	enum SectionModifier modifier;
	struct FileStackNode *src; /* Where the section was defined */
	uint32_t fileLine; /* Line where the section was defined */
	uint32_t size;
	uint32_t org;
	uint32_t bank;
	uint8_t align;
	uint16_t alignOfs;
	struct Section *next;
	struct Patch *patches;
	uint8_t *data;
};

struct SectionSpec {
	uint32_t bank;
	uint8_t alignment;
	uint16_t alignOfs;
};

struct Section *out_FindSectionByName(const char *name);
void out_NewSection(char const *name, uint32_t secttype, uint32_t org,
		    struct SectionSpec const *attributes,
		    enum SectionModifier mod);
void out_SetLoadSection(char const *name, uint32_t secttype, uint32_t org,
			struct SectionSpec const *attributes,
			enum SectionModifier mod);
void out_EndLoadSection(void);

struct Section *sect_GetSymbolSection(void);
uint32_t sect_GetSymbolOffset(void);
uint32_t sect_GetOutputOffset(void);
void sect_AlignPC(uint8_t alignment, uint16_t offset);

void sect_StartUnion(void);
void sect_NextUnionMember(void);
void sect_EndUnion(void);
void sect_CheckUnionClosed(void);

void out_AbsByte(uint8_t b);
void out_AbsByteGroup(uint8_t const *s, int32_t length);
void out_AbsWordGroup(uint8_t const *s, int32_t length);
void out_AbsLongGroup(uint8_t const *s, int32_t length);
void out_Skip(int32_t skip, bool ds);
void out_String(char const *s);
void out_RelByte(struct Expression const *expr, uint32_t pcShift);
void out_RelBytes(uint32_t n, struct Expression * const *exprs, size_t size);
void out_RelWord(struct Expression const *expr, uint32_t pcShift);
void out_RelLong(struct Expression const *expr, uint32_t pcShift);
void out_BinaryFile(char const *s, int32_t startPos);
void out_BinaryFileSlice(char const *s, int32_t start_pos, int32_t length);

void out_PushSection(void);
void out_PopSection(void);

#endif
