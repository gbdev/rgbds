/*
 * Outputs an objectfile
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/asm.h"
#include "asm/charmap.h"
#include "asm/output.h"
#include "asm/symbol.h"
#include "asm/mylink.h"
#include "asm/main.h"
#include "asm/rpn.h"
#include "asm/fstack.h"
#include "extern/err.h"

#define SECTIONCHUNK	0x4000

void out_SetCurrentSection(struct Section * pSect);

struct Patch {
	char tzFilename[_MAX_PATH + 1];
	ULONG nLine;
	ULONG nOffset;
	UBYTE nType;
	ULONG nRPNSize;
	UBYTE *pRPN;
	struct Patch *pNext;
};

struct PatchSymbol {
	ULONG ID;
	struct sSymbol *pSymbol;
	struct PatchSymbol *pNext;
	struct PatchSymbol *pBucketNext; // next symbol in hash table bucket
};

struct SectionStackEntry {
	struct Section *pSection;
	struct SectionStackEntry *pNext;
};

struct PatchSymbol *tHashedPatchSymbols[HASHSIZE];
struct Section *pSectionList = NULL, *pCurrentSection = NULL;
struct PatchSymbol *pPatchSymbols = NULL;
struct PatchSymbol **ppPatchSymbolsTail = &pPatchSymbols;
char *tzObjectname;
struct SectionStackEntry *pSectionStack = NULL;

/*
 * Section stack routines
 */
void 
out_PushSection(void)
{
	struct SectionStackEntry *pSect;

	if ((pSect = malloc(sizeof(struct SectionStackEntry))) != NULL) {
		pSect->pSection = pCurrentSection;
		pSect->pNext = pSectionStack;
		pSectionStack = pSect;
	} else
		fatalerror("No memory for section stack");
}

void 
out_PopSection(void)
{
	if (pSectionStack) {
		struct SectionStackEntry *pSect;

		pSect = pSectionStack;
		out_SetCurrentSection(pSect->pSection);
		pSectionStack = pSect->pNext;
		free(pSect);
	} else
		fatalerror("No entries in the section stack");
}

/*
 * Count the number of symbols used in this object
 */
ULONG 
countsymbols(void)
{
	struct PatchSymbol *pSym;
	ULONG count = 0;

	pSym = pPatchSymbols;

	while (pSym) {
		count += 1;
		pSym = pSym->pNext;
	}

	return (count);
}

/*
 * Count the number of sections used in this object
 */
ULONG 
countsections(void)
{
	struct Section *pSect;
	ULONG count = 0;

	pSect = pSectionList;

	while (pSect) {
		count += 1;
		pSect = pSect->pNext;
	}

	return (count);
}

/*
 * Count the number of patches used in this object
 */
ULONG 
countpatches(struct Section * pSect)
{
	struct Patch *pPatch;
	ULONG r = 0;

	pPatch = pSect->pPatches;
	while (pPatch) {
		r += 1;
		pPatch = pPatch->pNext;
	}

	return (r);
}

/*
 * Write a long to a file (little-endian)
 */
void 
fputlong(ULONG i, FILE * f)
{
	fputc(i, f);
	fputc(i >> 8, f);
	fputc(i >> 16, f);
	fputc(i >> 24, f);
}

/*
 * Write a NULL-terminated string to a file
 */
void 
fputstring(char *s, FILE * f)
{
	while (*s)
		fputc(*s++, f);
	fputc(0, f);
}

/*
 * Return a section's ID
 */
ULONG 
getsectid(struct Section * pSect)
{
	struct Section *sec;
	ULONG ID = 0;

	sec = pSectionList;

	while (sec) {
		if (sec == pSect)
			return (ID);
		ID += 1;
		sec = sec->pNext;
	}

	fatalerror("INTERNAL: Unknown section");
	return ((ULONG) - 1);
}

/*
 * Write a patch to a file
 */
void 
writepatch(struct Patch * pPatch, FILE * f)
{
	fputstring(pPatch->tzFilename, f);
	fputlong(pPatch->nLine, f);
	fputlong(pPatch->nOffset, f);
	fputc(pPatch->nType, f);
	fputlong(pPatch->nRPNSize, f);
	fwrite(pPatch->pRPN, 1, pPatch->nRPNSize, f);
}

/*
 * Write a section to a file
 */
void 
writesection(struct Section * pSect, FILE * f)
{
	fputstring(pSect->pzName, f); // RGB3 addition
	fputlong(pSect->nPC, f);
	fputc(pSect->nType, f);
	fputlong(pSect->nOrg, f);
	//RGB1 addition

	fputlong(pSect->nBank, f);
	//RGB1 addition
		
	fputlong(pSect->nAlign, f); // RGB3 addition

	if ((pSect->nType == SECT_ROM0)
	    || (pSect->nType == SECT_ROMX)) {
		struct Patch *pPatch;

		fwrite(pSect->tData, 1, pSect->nPC, f);
		fputlong(countpatches(pSect), f);

		pPatch = pSect->pPatches;
		while (pPatch) {
			writepatch(pPatch, f);
			pPatch = pPatch->pNext;
		}
	}
}

/*
 * Write a symbol to a file
 */
void 
writesymbol(struct sSymbol * pSym, FILE * f)
{
	char symname[MAXSYMLEN * 2 + 1];
	ULONG type;
	ULONG offset;
	SLONG sectid;

	if (pSym->nType & SYMF_IMPORT) {
		/* Symbol should be imported */
		strcpy(symname, pSym->tzName);
		offset = 0;
		sectid = -1;
		type = SYM_IMPORT;
	} else {
		if (pSym->nType & SYMF_LOCAL) {
			strcpy(symname, pSym->pScope->tzName);
			strcat(symname, pSym->tzName);
		} else
			strcpy(symname, pSym->tzName);
		
		if (pSym->nType & SYMF_EXPORT) {
			/* Symbol should be exported */
			type = SYM_EXPORT;
			offset = pSym->nValue;
			if (pSym->nType & SYMF_CONST)
				sectid = -1;
			else
				sectid = getsectid(pSym->pSection);
		} else {
			/* Symbol is local to this file */
			type = SYM_LOCAL;
			offset = pSym->nValue;
			sectid = getsectid(pSym->pSection);
		}
	}

	fputstring(symname, f);
	fputc(type, f);

	if (type != SYM_IMPORT) {
		fputlong(sectid, f);
		fputlong(offset, f);
	}
}

/*
 * Add a symbol to the object
 */
ULONG 
addsymbol(struct sSymbol * pSym)
{
	struct PatchSymbol *pPSym, **ppPSym;
	static ULONG nextID = 0;
	ULONG hash;
	

	hash = calchash(pSym->tzName);
	ppPSym = &(tHashedPatchSymbols[hash]);

	while ((*ppPSym) != NULL) {
		if (pSym == (*ppPSym)->pSymbol)
			return (*ppPSym)->ID;
		ppPSym = &((*ppPSym)->pBucketNext);
	}

	if ((*ppPSym = pPSym = malloc(sizeof(struct PatchSymbol))) != NULL) {
		pPSym->pNext = NULL;
		pPSym->pBucketNext = NULL;
		pPSym->pSymbol = pSym;
		pPSym->ID = nextID++;
	} else
		fatalerror("No memory for patchsymbol");

	*ppPatchSymbolsTail = pPSym;
	ppPatchSymbolsTail = &(pPSym->pNext);

	return pPSym->ID;
}

/*
 * Add all exported symbols to the object
 */
void 
addexports(void)
{
	int i;

	for (i = 0; i < HASHSIZE; i += 1) {
		struct sSymbol *pSym;

		pSym = tHashedSymbols[i];
		while (pSym) {
			if (pSym->nType & SYMF_EXPORT)
				addsymbol(pSym);
			pSym = pSym->pNext;
		}
	}
}

/*
 * Allocate a new patchstructure and link it into the list
 */
struct Patch *
allocpatch(void)
{
	struct Patch *pPatch;

	if ((pPatch = malloc(sizeof(struct Patch))) != NULL) {
		pPatch->pNext = pCurrentSection->pPatches;
		pPatch->nRPNSize = 0;
		pPatch->pRPN = NULL;
	} else
		fatalerror("No memory for patch");

	pCurrentSection->pPatches = pPatch;

	return (pPatch);
}

/*
 * Create a new patch (includes the rpn expr)
 */
void 
createpatch(ULONG type, struct Expression * expr)
{
	struct Patch *pPatch;
	UWORD rpndata;
	UBYTE rpnexpr[2048];
	char tzSym[512];
	ULONG rpnptr = 0, symptr;

	pPatch = allocpatch();
	pPatch->nType = type;
	strcpy(pPatch->tzFilename, tzCurrentFileName);
	pPatch->nLine = nLineNo;
	pPatch->nOffset = nPC;

	while ((rpndata = rpn_PopByte(expr)) != 0xDEAD) {
		switch (rpndata) {
		case RPN_CONST:
			rpnexpr[rpnptr++] = RPN_CONST;
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			rpnexpr[rpnptr++] = rpn_PopByte(expr);
			break;
		case RPN_SYM:
			symptr = 0;
			while ((tzSym[symptr++] = rpn_PopByte(expr)) != 0);
			if (sym_isConstant(tzSym)) {
				ULONG value;

				value = sym_GetConstantValue(tzSym);
				rpnexpr[rpnptr++] = RPN_CONST;
				rpnexpr[rpnptr++] = value & 0xFF;
				rpnexpr[rpnptr++] = value >> 8;
				rpnexpr[rpnptr++] = value >> 16;
				rpnexpr[rpnptr++] = value >> 24;
			} else {
				struct sSymbol *sym;
				if ((sym = sym_FindSymbol(tzSym)) == NULL)
					break;
				symptr = addsymbol(sym);
				rpnexpr[rpnptr++] = RPN_SYM;
				rpnexpr[rpnptr++] = symptr & 0xFF;
				rpnexpr[rpnptr++] = symptr >> 8;
				rpnexpr[rpnptr++] = symptr >> 16;
				rpnexpr[rpnptr++] = symptr >> 24;
			}
			break;
		case RPN_BANK: {
			struct sSymbol *sym;
			symptr = 0;
			while ((tzSym[symptr++] = rpn_PopByte(expr)) != 0);
			if ((sym = sym_FindSymbol(tzSym)) == NULL)
				break;
			symptr = addsymbol(sym);
			rpnexpr[rpnptr++] = RPN_BANK;
			rpnexpr[rpnptr++] = symptr & 0xFF;
			rpnexpr[rpnptr++] = symptr >> 8;
			rpnexpr[rpnptr++] = symptr >> 16;
			rpnexpr[rpnptr++] = symptr >> 24;
			}
			break;
		default:
			rpnexpr[rpnptr++] = rpndata;
			break;
		}
	}
	if ((pPatch->pRPN = malloc(rpnptr)) != NULL) {
		memcpy(pPatch->pRPN, rpnexpr, rpnptr);
		pPatch->nRPNSize = rpnptr;
	}
}

/*
 * A quick check to see if we have an initialized section
 */
void 
checksection(void)
{
	if (pCurrentSection)
		return;
	else
		fatalerror("Code generation before SECTION directive");
}

/*
 * A quick check to see if we have an initialized section that can contain
 * this much initialized data
 */
void 
checkcodesection(SLONG size)
{
	checksection();
	if (pCurrentSection->nType != SECT_ROM0 &&
	    pCurrentSection->nType != SECT_ROMX) {
		errx(1, "Section '%s' cannot contain code or data (not a "
		    "ROM0 or ROMX)", pCurrentSection->pzName);
	}
	if (pCurrentSection->nPC + size > MAXSECTIONSIZE) {
		/*
		 * N.B.: This check is not sufficient to ensure the section
		 * will fit, because there can be multiple sections of this
		 * type. The definitive check must be done at the linking
		 * stage.
		 */
		errx(1, "Section '%s' is too big (old size %d + %d > %d)",
		    pCurrentSection->pzName, pCurrentSection->nPC, size,
		    MAXSECTIONSIZE);
	}
	if (((pCurrentSection->nPC % SECTIONCHUNK) >
	    ((pCurrentSection->nPC + size) % SECTIONCHUNK)) &&
	    (pCurrentSection->nType == SECT_ROM0 ||
	    pCurrentSection->nType == SECT_ROMX)) {
		pCurrentSection->tData = realloc(pCurrentSection->tData,
		    ((pCurrentSection->nPC + size) / SECTIONCHUNK + 1) *
		    SECTIONCHUNK);

		if (pCurrentSection->tData == NULL) {
			err(1, "Could not expand section");
		}
	}
	return;
}

/*
 * Write an objectfile
 */
void 
out_WriteObject(void)
{
	FILE *f;

	addexports();

	if ((f = fopen(tzObjectname, "wb")) != NULL) {
		struct PatchSymbol *pSym;
		struct Section *pSect;

		fwrite("RGB4", 1, 4, f);
		fputlong(countsymbols(), f);
		fputlong(countsections(), f);

		pSym = pPatchSymbols;
		while (pSym) {
			writesymbol(pSym->pSymbol, f);
			pSym = pSym->pNext;
		}

		pSect = pSectionList;
		while (pSect) {
			writesection(pSect, f);
			pSect = pSect->pNext;
		}

		fclose(f);
	}
}

/*
 * Prepare for pass #2
 */
void 
out_PrepPass2(void)
{
	struct Section *pSect;

	pSect = pSectionList;
	while (pSect) {
		pSect->nPC = 0;
		pSect = pSect->pNext;
	}
	pCurrentSection = NULL;
	pSectionStack = NULL;
}

/*
 * Set the objectfilename
 */
void 
out_SetFileName(char *s)
{
	tzObjectname = s;
	if (CurrentOptions.verbose) {
		printf("Output filename %s\n", s);
	}
	pSectionList = NULL;
	pCurrentSection = NULL;
	pPatchSymbols = NULL;
}

/*
 * Find a section by name and type.  If it doesn't exist, create it
 */
struct Section *
out_FindSection(char *pzName, ULONG secttype, SLONG org, SLONG bank, SLONG alignment)
{
	struct Section *pSect, **ppSect;

	ppSect = &pSectionList;
	pSect = pSectionList;

	while (pSect) {
		if (strcmp(pzName, pSect->pzName) == 0) {
			if (secttype == pSect->nType
			    && ((ULONG) org) == pSect->nOrg
			    && ((ULONG) bank) == pSect->nBank
			    && ((ULONG) alignment == pSect->nAlign)) {
				return (pSect);
			} else
				fatalerror
				    ("Section already exists but with a different type");
		}
		ppSect = &(pSect->pNext);
		pSect = pSect->pNext;
	}

	if ((*ppSect = (pSect = malloc(sizeof(struct Section)))) != NULL) {
		if ((pSect->pzName = malloc(strlen(pzName) + 1)) != NULL) {
			strcpy(pSect->pzName, pzName);
			pSect->nType = secttype;
			pSect->nPC = 0;
			pSect->nOrg = org;
			pSect->nBank = bank;
			pSect->nAlign = alignment;
			pSect->pNext = NULL;
			pSect->pPatches = NULL;
			pSect->charmap = NULL;
			pPatchSymbols = NULL;

			if ((pSect->tData = malloc(SECTIONCHUNK)) != NULL) {
				return (pSect);
			} else
				fatalerror("Not enough memory for section");
		} else
			fatalerror("Not enough memory for sectionname");
	} else
		fatalerror("Not enough memory for section");

	return (NULL);
}

/*
 * Set the current section
 */
void 
out_SetCurrentSection(struct Section * pSect)
{
	pCurrentSection = pSect;
	nPC = pSect->nPC;

	pPCSymbol->nValue = nPC;
	pPCSymbol->pSection = pCurrentSection;
}

/*
 * Set the current section by name and type
 */
void 
out_NewSection(char *pzName, ULONG secttype)
{
	out_SetCurrentSection(out_FindSection(pzName, secttype, -1, -1, 1));
}

/*
 * Set the current section by name and type
 */
void 
out_NewAbsSection(char *pzName, ULONG secttype, SLONG org, SLONG bank)
{
	out_SetCurrentSection(out_FindSection(pzName, secttype, org, bank, 1));
}

/*
 * Set the current section by name and type, using a given byte alignment
 */
void 
out_NewAlignedSection(char *pzName, ULONG secttype, SLONG alignment, SLONG bank)
{
	if (alignment < 0 || alignment > 16) {
		yyerror("Alignment must be between 0-16 bits.");
	}
	out_SetCurrentSection(out_FindSection(pzName, secttype, -1, bank, 1 << alignment));
}

/*
 * Output an absolute byte
 */
void 
out_AbsByte(int b)
{
	checkcodesection(1);
	b &= 0xFF;
	if (nPass == 2)
		pCurrentSection->tData[nPC] = b;

	pCurrentSection->nPC += 1;
	nPC += 1;
	pPCSymbol->nValue += 1;
}

void 
out_AbsByteGroup(char *s, int length)
{
	checkcodesection(length);
	while (length--)
		out_AbsByte(*s++);
}

/*
 * Skip this many bytes
 */
void 
out_Skip(int skip)
{
	checksection();
	if (!((pCurrentSection->nType == SECT_ROM0)
		|| (pCurrentSection->nType == SECT_ROMX))) {
		pCurrentSection->nPC += skip;
		nPC += skip;
		pPCSymbol->nValue += skip;
	} else {
		checkcodesection(skip);
		while (skip--)
			out_AbsByte(CurrentOptions.fillchar);
	}
}

/*
 * Output a NULL terminated string (excluding the NULL-character)
 */
void 
out_String(char *s)
{
	checkcodesection(strlen(s));
	while (*s)
		out_AbsByte(*s++);
}

/*
 * Output a relocatable byte.  Checking will be done to see if it
 * is an absolute value in disguise.
 */

void 
out_RelByte(struct Expression * expr)
{
	checkcodesection(1);
	if (rpn_isReloc(expr)) {
		if (nPass == 2) {
			pCurrentSection->tData[nPC] = 0;
			createpatch(PATCH_BYTE, expr);
		}
		pCurrentSection->nPC += 1;
		nPC += 1;
		pPCSymbol->nValue += 1;
	} else
		out_AbsByte(expr->nVal);

	rpn_Reset(expr);
}

/*
 * Output an absolute word
 */
void 
out_AbsWord(int b)
{
	checkcodesection(2);
	b &= 0xFFFF;
	if (nPass == 2) {
		pCurrentSection->tData[nPC] = b & 0xFF;
		pCurrentSection->tData[nPC + 1] = b >> 8;
	}
	pCurrentSection->nPC += 2;
	nPC += 2;
	pPCSymbol->nValue += 2;
}

/*
 * Output a relocatable word.  Checking will be done to see if
 * it's an absolute value in disguise.
 */
void 
out_RelWord(struct Expression * expr)
{
	ULONG b;

	checkcodesection(2);
	b = expr->nVal & 0xFFFF;
	if (rpn_isReloc(expr)) {
		if (nPass == 2) {
			pCurrentSection->tData[nPC] = b & 0xFF;
			pCurrentSection->tData[nPC + 1] = b >> 8;
			createpatch(PATCH_WORD_L, expr);
		}
		pCurrentSection->nPC += 2;
		nPC += 2;
		pPCSymbol->nValue += 2;
	} else
		out_AbsWord(expr->nVal);
	rpn_Reset(expr);
}

/*
 * Output an absolute longword
 */
void 
out_AbsLong(SLONG b)
{
	checkcodesection(sizeof(SLONG));
	if (nPass == 2) {
		pCurrentSection->tData[nPC] = b & 0xFF;
		pCurrentSection->tData[nPC + 1] = b >> 8;
		pCurrentSection->tData[nPC + 2] = b >> 16;
		pCurrentSection->tData[nPC + 3] = b >> 24;
	}
	pCurrentSection->nPC += 4;
	nPC += 4;
	pPCSymbol->nValue += 4;
}

/*
 * Output a relocatable longword.  Checking will be done to see if
 * is an absolute value in disguise.
 */
void 
out_RelLong(struct Expression * expr)
{
	SLONG b;

	checkcodesection(4);
	b = expr->nVal;
	if (rpn_isReloc(expr)) {
		if (nPass == 2) {
			pCurrentSection->tData[nPC] = b & 0xFF;
			pCurrentSection->tData[nPC + 1] = b >> 8;
			pCurrentSection->tData[nPC + 2] = b >> 16;
			pCurrentSection->tData[nPC + 3] = b >> 24;
			createpatch(PATCH_LONG_L, expr);
		}
		pCurrentSection->nPC += 4;
		nPC += 4;
		pPCSymbol->nValue += 4;
	} else
		out_AbsLong(expr->nVal);
	rpn_Reset(expr);
}

/*
 * Output a PC-relative byte
 */
void 
out_PCRelByte(struct Expression * expr)
{
	SLONG b = expr->nVal;

	checkcodesection(1);
	b = (b & 0xFFFF) - (nPC + 1);
	if (nPass == 2 && (b < -128 || b > 127))
		yyerror("PC-relative value must be 8-bit");

	out_AbsByte(b);
	rpn_Reset(expr);
}

/*
 * Output a binary file
 */
void 
out_BinaryFile(char *s)
{
	FILE *f;

	f = fstk_FindFile(s);
	if (f == NULL) {
		err(1, "Unable to open incbin file '%s'", s);
	}

	SLONG fsize;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	checkcodesection(fsize);

	if (nPass == 2) {
		SLONG dest = nPC;
		SLONG todo = fsize;

		while (todo--)
			pCurrentSection->tData[dest++] = fgetc(f);
	}
	pCurrentSection->nPC += fsize;
	nPC += fsize;
	pPCSymbol->nValue += fsize;
	fclose(f);
}

void 
out_BinaryFileSlice(char *s, SLONG start_pos, SLONG length)
{
	FILE *f;

	if (start_pos < 0)
		fatalerror("Start position cannot be negative");

	if (length < 0)
		fatalerror("Number of bytes to read must be greater than zero");

	f = fstk_FindFile(s);
	if (f == NULL) {
		err(1, "Unable to open included file '%s'", s);
	}

	SLONG fsize;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);

	if (start_pos >= fsize)
		fatalerror("Specified start position is greater than length of file");

	if ((start_pos + length) > fsize)
		fatalerror("Specified range in INCBIN is out of bounds");

	fseek(f, start_pos, SEEK_SET);

	checkcodesection(length);

	if (nPass == 2) {
		SLONG dest = nPC;
		SLONG todo = length;

		while (todo--)
			pCurrentSection->tData[dest++] = fgetc(f);
	}
	pCurrentSection->nPC += length;
	nPC += length;
	pPCSymbol->nValue += length;

	fclose(f);
}
