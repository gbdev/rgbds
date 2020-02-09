
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/fstack.h"
#include "asm/main.h"
#include "asm/output.h"
#include "asm/rpn.h"
#include "asm/section.h"
#include "asm/warning.h"

#include "extern/err.h"

struct SectionStackEntry {
	struct Section *pSection;
	struct sSymbol *pScope; /* Section's symbol scope */
	struct SectionStackEntry *pNext;
};

struct SectionStackEntry *pSectionStack;

/*
 * A quick check to see if we have an initialized section
 */
static void checksection(void)
{
	if (pCurrentSection == NULL)
		fatalerror("Code generation before SECTION directive");
}

/*
 * A quick check to see if we have an initialized section that can contain
 * this much initialized data
 */
static void checkcodesection(void)
{
	checksection();

	if (!sect_HasData(pCurrentSection->nType))
		fatalerror("Section '%s' cannot contain code or data (not ROM0 or ROMX)",
			   pCurrentSection->pzName);
	else if (nUnionDepth > 0)
		fatalerror("UNIONs cannot contain code or data");
}

/*
 * Check if the section has grown too much.
 */
static void checksectionoverflow(uint32_t delta_size)
{
	uint32_t maxSize = maxsize[pCurrentSection->nType];
	uint32_t newSize = pCurrentSection->nPC + delta_size;

	if (newSize > maxSize) {
		/*
		 * This check is here to trap broken code that generates
		 * sections that are too big and to prevent the assembler from
		 * generating huge object files or trying to allocate too much
		 * memory.
		 * The real check must be done at the linking stage.
		 */
		fatalerror("Section '%s' is too big (max size = 0x%X bytes, reached 0x%X).",
			   pCurrentSection->pzName, maxSize, newSize);
	}
}

struct Section *out_FindSectionByName(const char *pzName)
{
	struct Section *pSect = pSectionList;

	while (pSect) {
		if (strcmp(pzName, pSect->pzName) == 0)
			return pSect;

		pSect = pSect->pNext;
	}

	return NULL;
}

/*
 * Find a section by name and type. If it doesn't exist, create it
 */
static struct Section *findSection(char const *pzName, enum SectionType type,
				   int32_t org, int32_t bank, int32_t alignment)
{
	struct Section *pSect = out_FindSectionByName(pzName);

	if (pSect) {
		if (type == pSect->nType
			&& ((uint32_t)org) == pSect->nOrg
			&& ((uint32_t)bank) == pSect->nBank
			&& ((uint32_t)alignment == pSect->nAlign)) {
			return pSect;
		}
		fatalerror("Section already exists but with a different type");
	}

	pSect = malloc(sizeof(*pSect));
	if (pSect == NULL)
		fatalerror("Not enough memory for section");

	pSect->pzName = strdup(pzName);
	if (pSect->pzName == NULL)
		fatalerror("Not enough memory for sectionname");

	if (nbbanks(type) == 1)
		bank = bankranges[type][0];

	pSect->nType = type;
	pSect->nPC = 0;
	pSect->nOrg = org;
	pSect->nBank = bank;
	pSect->nAlign = alignment;
	pSect->pNext = pSectionList;
	pSect->pPatches = NULL;

	/* It is only needed to allocate memory for ROM sections. */
	if (sect_HasData(type)) {
		uint32_t sectsize;

		sectsize = maxsize[type];
		pSect->tData = malloc(sectsize);
		if (pSect->tData == NULL)
			fatalerror("Not enough memory for section");
	} else {
		pSect->tData = NULL;
	}

	/*
	 * Add the new section to the list
	 * at the beginning because order doesn't matter
	 */
	pSectionList = pSect;

	return pSect;
}

/*
 * Set the current section
 */
static void setCurrentSection(struct Section *pSect)
{
	if (nUnionDepth > 0)
		fatalerror("Cannot change the section within a UNION");

	pCurrentSection = pSect;
	nPC = (pSect != NULL) ? pSect->nPC : 0;

	pPCSymbol->pSection = pCurrentSection;
	pPCSymbol->isConstant = pSect && pSect->nOrg != -1;
}

/*
 * Set the current section by name and type
 */
void out_NewSection(char const *pzName, uint32_t secttype, int32_t org,
		    struct SectionSpec const *attributes)
{
	uint32_t align = 1 << attributes->alignment;

	if (attributes->bank != -1) {
		if (secttype != SECTTYPE_ROMX && secttype != SECTTYPE_VRAM
		 && secttype != SECTTYPE_SRAM && secttype != SECTTYPE_WRAMX)
			yyerror("BANK only allowed for ROMX, WRAMX, SRAM, or VRAM sections");
		else if (attributes->bank < bankranges[secttype][0]
		      || attributes->bank > bankranges[secttype][1])
			yyerror("%s bank value $%x out of range ($%x to $%x)",
				typeNames[secttype], attributes->bank,
				bankranges[secttype][0],
				bankranges[secttype][1]);
	}

	if (align != 1) {
		/* It doesn't make sense to have both set */
		uint32_t mask = align - 1;

		if (org != -1) {
			if (org & mask)
				yyerror("Section \"%s\"'s fixed address doesn't match its alignment",
					pzName);
			else
				align = 1; /* Ignore it if it's satisfied */
		}
	}

	if (org != -1) {
		if (org < startaddr[secttype] || org > endaddr(secttype))
			yyerror("Section \"%s\"'s fixed address %#x is outside of range [%#x; %#x]",
				pzName, org, startaddr[secttype],
				endaddr(secttype));
	}

	setCurrentSection(findSection(pzName, secttype, org, attributes->bank,
				      1 << attributes->alignment));
}

/*
 * Output an absolute byte (bypassing ROM/union checks)
 */
static void absByteBypassCheck(int32_t b)
{
	b &= 0xFF;
	pCurrentSection->tData[nPC] = b;
	pCurrentSection->nPC++;
	nPC++;
}

/*
 * Output an absolute byte
 */
void out_AbsByte(int32_t b)
{
	checkcodesection();
	checksectionoverflow(1);
	absByteBypassCheck(b);
}

void out_AbsByteGroup(char const *s, int32_t length)
{
	checkcodesection();
	checksectionoverflow(length);
	while (length--)
		absByteBypassCheck(*s++);
}

/*
 * Skip this many bytes
 */
void out_Skip(int32_t skip)
{
	checksection();
	checksectionoverflow(skip);
	if (!sect_HasData(pCurrentSection->nType)) {
		pCurrentSection->nPC += skip;
		nPC += skip;
	} else if (nUnionDepth > 0) {
		while (skip--)
			absByteBypassCheck(CurrentOptions.fillchar);
	} else {
		checkcodesection();
		while (skip--)
			absByteBypassCheck(CurrentOptions.fillchar);
	}
}

/*
 * Output a NULL terminated string (excluding the NULL-character)
 */
void out_String(char const *s)
{
	checkcodesection();
	checksectionoverflow(strlen(s));
	while (*s)
		absByteBypassCheck(*s++);
}

/*
 * Output a relocatable byte. Checking will be done to see if it
 * is an absolute value in disguise.
 */
void out_RelByte(struct Expression *expr)
{
	checkcodesection();
	checksectionoverflow(1);
	if (!rpn_isKnown(expr)) {
		pCurrentSection->tData[nPC] = 0;
		out_CreatePatch(PATCHTYPE_BYTE, expr);
		pCurrentSection->nPC++;
		nPC++;
	} else {
		absByteBypassCheck(expr->nVal);
	}
	rpn_Free(expr);
}

/*
 * Output an absolute word
 */
static void absWord(int32_t b)
{
	checkcodesection();
	checksectionoverflow(2);
	b &= 0xFFFF;
	pCurrentSection->tData[nPC] = b & 0xFF;
	pCurrentSection->tData[nPC + 1] = b >> 8;
	pCurrentSection->nPC += 2;
	nPC += 2;
}

/*
 * Output a relocatable word. Checking will be done to see if
 * it's an absolute value in disguise.
 */
void out_RelWord(struct Expression *expr)
{
	checkcodesection();
	checksectionoverflow(2);
	if (!rpn_isKnown(expr)) {
		pCurrentSection->tData[nPC] = 0;
		pCurrentSection->tData[nPC + 1] = 0;
		out_CreatePatch(PATCHTYPE_WORD, expr);
		pCurrentSection->nPC += 2;
		nPC += 2;
	} else {
		absWord(expr->nVal);
	}
	rpn_Free(expr);
}

/*
 * Output an absolute longword
 */
static void absLong(int32_t b)
{
	checkcodesection();
	checksectionoverflow(sizeof(int32_t));
	pCurrentSection->tData[nPC] = b & 0xFF;
	pCurrentSection->tData[nPC + 1] = b >> 8;
	pCurrentSection->tData[nPC + 2] = b >> 16;
	pCurrentSection->tData[nPC + 3] = b >> 24;
	pCurrentSection->nPC += 4;
	nPC += 4;
}

/*
 * Output a relocatable longword. Checking will be done to see if
 * is an absolute value in disguise.
 */
void out_RelLong(struct Expression *expr)
{
	checkcodesection();
	checksectionoverflow(4);
	if (!rpn_isKnown(expr)) {
		pCurrentSection->tData[nPC] = 0;
		pCurrentSection->tData[nPC + 1] = 0;
		pCurrentSection->tData[nPC + 2] = 0;
		pCurrentSection->tData[nPC + 3] = 0;
		out_CreatePatch(PATCHTYPE_LONG, expr);
		pCurrentSection->nPC += 4;
		nPC += 4;
	} else {
		absLong(expr->nVal);
	}
	rpn_Free(expr);
}

/*
 * Output a PC-relative relocatable byte. Checking will be done to see if it
 * is an absolute value in disguise.
 */
void out_PCRelByte(struct Expression *expr)
{
	checkcodesection();
	checksectionoverflow(1);
	if (!rpn_isKnown(expr) || pCurrentSection->nOrg == -1) {
		pCurrentSection->tData[nPC] = 0;
		out_CreatePatch(PATCHTYPE_JR, expr);
		pCurrentSection->nPC++;
		nPC++;
	} else {
		/* Target is relative to the byte *after* the operand */
		uint16_t address = pCurrentSection->nOrg + nPC + 1;
		/* The offset wraps (jump from ROM to HRAM, for loopexample) */
		int16_t offset = expr->nVal - address;

		if (offset < -128 || offset > 127) {
			yyerror("jr target out of reach (expected -129 < %d < 128)", offset);
			out_AbsByte(0);
		} else {
			out_AbsByte(offset);
		}
	}
	rpn_Free(expr);
}

/*
 * Output a binary file
 */
void out_BinaryFile(char const *s)
{
	FILE *f;

	f = fstk_FindFile(s, NULL);
	if (f == NULL) {
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		err(1, "Unable to open incbin file '%s'", s);
	}

	int32_t fsize;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	checkcodesection();
	checksectionoverflow(fsize);

	int32_t dest = nPC;
	int32_t todo = fsize;

	while (todo--)
		pCurrentSection->tData[dest++] = fgetc(f);

	pCurrentSection->nPC += fsize;
	nPC += fsize;
	fclose(f);
}

void out_BinaryFileSlice(char const *s, int32_t start_pos, int32_t length)
{
	FILE *f;

	if (start_pos < 0)
		fatalerror("Start position cannot be negative");

	if (length < 0)
		fatalerror("Number of bytes to read must be greater than zero");

	f = fstk_FindFile(s, NULL);
	if (f == NULL) {
		if (oGeneratedMissingIncludes) {
			oFailedOnMissingInclude = true;
			return;
		}
		err(1, "Unable to open included file '%s'", s);
	}

	int32_t fsize;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);

	if (start_pos >= fsize)
		fatalerror("Specified start position is greater than length of file");

	if ((start_pos + length) > fsize)
		fatalerror("Specified range in INCBIN is out of bounds");

	fseek(f, start_pos, SEEK_SET);

	checkcodesection();
	checksectionoverflow(length);

	int32_t dest = nPC;
	int32_t todo = length;

	while (todo--)
		pCurrentSection->tData[dest++] = fgetc(f);

	pCurrentSection->nPC += length;
	nPC += length;

	fclose(f);
}

/*
 * Section stack routines
 */
void out_PushSection(void)
{
	struct SectionStackEntry *pSect;

	pSect = malloc(sizeof(struct SectionStackEntry));
	if (pSect == NULL)
		fatalerror("No memory for section stack");

	pSect->pSection = pCurrentSection;
	pSect->pScope = sym_GetCurrentSymbolScope();
	pSect->pNext = pSectionStack;
	pSectionStack = pSect;
}

void out_PopSection(void)
{
	if (pSectionStack == NULL)
		fatalerror("No entries in the section stack");

	struct SectionStackEntry *pSect;

	pSect = pSectionStack;
	setCurrentSection(pSect->pSection);
	sym_SetCurrentSymbolScope(pSect->pScope);
	pSectionStack = pSect->pNext;
	free(pSect);
}
