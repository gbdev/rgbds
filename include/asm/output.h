#ifndef RGBDS_ASM_OUTPUT_H
#define RGBDS_ASM_OUTPUT_H

#include "asm/rpn.h"
#include "types.h"

struct Section {
	char *pzName;
	UBYTE nType;
	ULONG nPC;
	ULONG nOrg;
	ULONG nBank;
	ULONG nAlign;
	struct Section *pNext;
	struct Patch *pPatches;
	struct Charmap *charmap;
	UBYTE *tData;
};

void out_PrepPass2(void);
void out_SetFileName(char *s);
void out_NewSection(char *pzName, ULONG secttype);
void out_NewAbsSection(char *pzName, ULONG secttype, SLONG org, SLONG bank);
void out_NewAlignedSection(char *pzName, ULONG secttype, SLONG alignment, SLONG bank);
void out_AbsByte(int b);
void out_AbsByteGroup(char *s, int length);
void out_RelByte(struct Expression * expr);
void out_RelWord(struct Expression * expr);
void out_PCRelByte(struct Expression * expr);
void out_WriteObject(void);
void out_Skip(int skip);
void out_BinaryFile(char *s);
void out_BinaryFileSlice(char *s, SLONG start_pos, SLONG length);
void out_String(char *s);
void out_AbsLong(SLONG b);
void out_RelLong(struct Expression * expr);
void out_PushSection(void);
void out_PopSection(void);

#endif
