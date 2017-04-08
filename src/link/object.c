/*
 * Here we have the routines that read an objectfile
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "link/assign.h"
#include "link/mylink.h"
#include "link/main.h"

struct sSymbol **tSymbols;
struct sSection *pSections = NULL;
struct sSection *pLibSections = NULL;
UBYTE dummymem;
BBOOL oReadLib = 0;

/*
 * The usual byte order stuff
 *
 */

SLONG
readlong(FILE * f)
{
	SLONG r;

	r = fgetc(f);
	r |= fgetc(f) << 8;
	r |= fgetc(f) << 16;
	r |= fgetc(f) << 24;

	return (r);
}

UWORD
readword(FILE * f)
{
	UWORD r;

	r = fgetc(f);
	r |= fgetc(f) << 8;

	return (r);
}

/*
 * Read a NULL terminated string from a file
 *
 */
SLONG
readasciiz(char **dest, FILE *f)
{
	size_t r = 0;

	size_t bufferLength = 16;
	char *start = malloc(bufferLength);
	char *s = start;

	if (!s) {
		err(1, NULL);
	}

	while (((*s++) = fgetc(f)) != 0) {
		r += 1;

		if (r >= bufferLength) {
			bufferLength *= 2;
			start = realloc(start, bufferLength);
			if (!start) {
				err(1, NULL);
			}
			s = start + r;
		}
	}

	*dest = start;
	return (r + 1);
}

/*
 * Allocate a new section and link it into the list
 *
 */

struct sSection *
AllocSection(void)
{
	struct sSection **ppSections;

	if (oReadLib == 1)
		ppSections = &pLibSections;
	else
		ppSections = &pSections;

	while (*ppSections)
		ppSections = &((*ppSections)->pNext);

	*ppSections = malloc(sizeof **ppSections);
	if (!*ppSections) {
		err(1, NULL);
	}
	(*ppSections)->tSymbols = tSymbols;
	(*ppSections)->pNext = NULL;
	(*ppSections)->pPatches = NULL;
	(*ppSections)->oAssigned = 0;
	return *ppSections;
}
/*
 * Read a symbol from a file
 *
 */

struct sSymbol *
obj_ReadSymbol(FILE * f)
{
	struct sSymbol *pSym;

	pSym = malloc(sizeof *pSym);
	if (!pSym) {
		err(1, NULL);
	}

	readasciiz(&pSym->pzName, f);
	if ((pSym->Type = (enum eSymbolType) fgetc(f)) != SYM_IMPORT) {
		pSym->nSectionID = readlong(f);
		pSym->nOffset = readlong(f);
	}
	return pSym;
}

/*
 * RGB object reader routines
 *
 */
struct sSection *
obj_ReadRGBSection(FILE * f)
{
	struct sSection *pSection;

	char *pzName;
	readasciiz(&pzName, f);
	if (IsSectionNameInUse(pzName))
		errx(1, "Section name \"%s\" is already in use.", pzName);

	pSection = AllocSection();
	pSection->pzName = pzName;

	pSection->nByteSize = readlong(f);
	pSection->Type = (enum eSectionType) fgetc(f);
	pSection->nOrg = readlong(f);
	pSection->nBank = readlong(f);
	pSection->nAlign = readlong(f);

	if ((options & OPT_TINY) && (pSection->Type == SECT_ROMX)) {
		errx(1,  "ROMX sections can't be used with option -t.");
	}
	if (options & OPT_DMG_MODE) {
		if (pSection->Type == SECT_WRAMX) {
			errx(1, "WRAMX sections can't be used with option -w.");
		}
		if (pSection->Type == SECT_VRAM && pSection->nBank == 1) {
			errx(1, "VRAM bank 1 can't be used with option -w.");
		}
	}

	if ((pSection->Type == SECT_ROMX) || (pSection->Type == SECT_ROM0)) {
		/*
		 * These sectiontypes contain data...
		 *
		 */
		if (pSection->nByteSize) {
			pSection->pData = malloc(pSection->nByteSize);
			if (!pSection->pData) {
				err(1, NULL);
			}

			SLONG nNumberOfPatches;
			struct sPatch **ppPatch, *pPatch;

			fread(pSection->pData, sizeof(UBYTE),
			    pSection->nByteSize, f);
			nNumberOfPatches = readlong(f);
			ppPatch = &pSection->pPatches;

			/*
			 * And patches...
			 *
			 */
			while (nNumberOfPatches--) {
				pPatch = malloc(sizeof *pPatch);
				if (!pPatch) {
					err(1, NULL);
				}

				*ppPatch = pPatch;
				readasciiz(&pPatch->pzFilename, f);
				pPatch->nLineNo = readlong(f);
				pPatch->nOffset = readlong(f);
				pPatch->Type = (enum ePatchType) fgetc(f);
				if ((pPatch->nRPNSize = readlong(f)) > 0) {
					pPatch->pRPN = malloc(pPatch->nRPNSize);
					if (!pPatch->pRPN) {
						err(1, NULL);
					}

					fread(pPatch->pRPN, sizeof(UBYTE),
					    pPatch->nRPNSize, f);
				} else
					pPatch->pRPN = NULL;

				pPatch->pNext = NULL;
				ppPatch = &(pPatch->pNext);
			}
		} else {
			/* Skip number of patches */
			readlong(f);
			pSection->pData = &dummymem;
		}
	}
	return pSection;
}

void
obj_ReadRGB(FILE * pObjfile)
{
	struct sSection *pFirstSection;
	SLONG nNumberOfSymbols, nNumberOfSections, i;

	nNumberOfSymbols = readlong(pObjfile);
	nNumberOfSections = readlong(pObjfile);

	/* First comes the symbols */

	if (nNumberOfSymbols) {
		tSymbols = malloc(nNumberOfSymbols * sizeof *tSymbols);
		if (!tSymbols) {
			err(1, NULL);
		}

		for (i = 0; i < nNumberOfSymbols; i += 1)
			tSymbols[i] = obj_ReadSymbol(pObjfile);
	} else
		tSymbols = (struct sSymbol **) & dummymem;

	/* Next we have the sections */

	pFirstSection = NULL;
	while (nNumberOfSections--) {
		struct sSection *pNewSection;

		pNewSection = obj_ReadRGBSection(pObjfile);
		pNewSection->nNumberOfSymbols = nNumberOfSymbols;
		if (pFirstSection == NULL)
			pFirstSection = pNewSection;
	}

	/*
	 * Fill in the pSection entry in the symbolstructure.
	 * This REALLY needs some cleaning up... but, hey, it works
	 *
	 */

	for (i = 0; i < nNumberOfSymbols; i += 1) {
		struct sSection *pConvSect = pFirstSection;

		if (tSymbols[i]->Type != SYM_IMPORT
		    && tSymbols[i]->nSectionID != -1) {
			SLONG j = 0;
			while (j != tSymbols[i]->nSectionID) {
				j += 1;
				pConvSect = pConvSect->pNext;
			}
			tSymbols[i]->pSection = pConvSect;
		} else
			tSymbols[i]->pSection = NULL;
	}
}
/*
 * The main objectfileloadroutine (phew)
 *
 */

void
obj_ReadOpenFile(FILE * pObjfile, char *tzObjectfile)
{
	char tzHeader[8];

	fread(tzHeader, sizeof(char), 4, pObjfile);
	tzHeader[4] = 0;
	if (strncmp(tzHeader, "RGB", 3) == 0) {
		switch (tzHeader[3]) {
		case '3':
		case '4': // V4 supports OAM sections, but is otherwise identical
			obj_ReadRGB(pObjfile);
			break;
		default:
			errx(1, "'%s' uses an unsupported object file version (%s). Please reassemble it.", tzObjectfile, tzHeader);
		}
	} else {
		errx(1, "'%s' is not a valid object", tzObjectfile);
	}
}

void
obj_Readfile(char *tzObjectfile)
{
	FILE *pObjfile;

	if (options & OPT_SMART_C_LINK)
		oReadLib = 1;
	else
		oReadLib = 0;

	pObjfile = fopen(tzObjectfile, "rb");
	if (pObjfile == NULL) {
		err(1, "Unable to open object '%s'", tzObjectfile);
	}
	obj_ReadOpenFile(pObjfile, tzObjectfile);
	fclose(pObjfile);

	oReadLib = 0;
}

SLONG
file_Length(FILE * f)
{
	ULONG r, p;

	p = ftell(f);
	fseek(f, 0, SEEK_END);
	r = ftell(f);
	fseek(f, p, SEEK_SET);

	return (r);
}

void
lib_ReadXLB0(FILE * f)
{
	SLONG size;

	size = file_Length(f) - 4;
	while (size) {
		char *name;

		size -= readasciiz(&name, f);
		readword(f);
		size -= 2;
		readword(f);
		size -= 2;
		size -= readlong(f);
		size -= 4;
		obj_ReadOpenFile(f, name);
		free(name);
	}
}
