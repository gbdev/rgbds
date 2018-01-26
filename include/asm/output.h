/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_OUTPUT_H
#define RGBDS_ASM_OUTPUT_H

#include <stdint.h>

#include "asm/rpn.h"

struct Section {
	char *pzName;
	uint8_t nType;
	uint32_t nPC;
	uint32_t nOrg;
	uint32_t nBank;
	uint32_t nAlign;
	struct Section *pNext;
	struct Patch *pPatches;
	struct Charmap *charmap;
	uint8_t *tData;
};

extern char *tzObjectname;

void out_PrepPass2(void);
void out_SetFileName(char *s);
void out_NewSection(char *pzName, uint32_t secttype);
void out_NewAbsSection(char *pzName, uint32_t secttype, int32_t org,
		       int32_t bank);
void out_NewAlignedSection(char *pzName, uint32_t secttype, int32_t alignment,
			   int32_t bank);
void out_AbsByte(int32_t b);
void out_AbsByteGroup(char *s, int32_t length);
void out_RelByte(struct Expression *expr);
void out_RelWord(struct Expression *expr);
void out_PCRelByte(struct Expression *expr);
void out_WriteObject(void);
void out_Skip(int32_t skip);
void out_BinaryFile(char *s);
void out_BinaryFileSlice(char *s, int32_t start_pos, int32_t length);
void out_String(char *s);
void out_AbsLong(int32_t b);
void out_RelLong(struct Expression *expr);
void out_PushSection(void);
void out_PopSection(void);

#endif /* RGBDS_ASM_OUTPUT_H */
