/*
 * Here we have the routines that read an objectfile
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "link/mylink.h"
#include "link/main.h"

struct sSymbol **tSymbols;
struct sSection *pSections = NULL;
struct sSection *pLibSections = NULL;
UBYTE dummymem;
BBOOL oReadLib = 0;

enum ObjectFileContents {
	CONTAINS_SECTION_NAME = 1 << 0,
	CONTAINS_SECTION_ALIGNMENT = 1 << 1
};

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
	SLONG r = 0;
	
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
 * RGB0 object reader routines
 *
 */

struct sSection *
obj_ReadRGB0Section(FILE * f)
{
	struct sSection *pSection;

	pSection = AllocSection();

	pSection->pzName = "";
	pSection->nByteSize = readlong(f);
	pSection->Type = (enum eSectionType) fgetc(f);
	pSection->nOrg = -1;
	pSection->nBank = -1;
	pSection->nAlign = 1;

	/* does the user want the -s mode? */

	if ((options & OPT_SMALL) && (pSection->Type == SECT_ROMX)) {
		pSection->Type = SECT_ROM0;
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

				pPatch->nLineNo =
				    readlong(f);
				pPatch->nOffset =
				    readlong(f);
				pPatch->Type =
				    (enum ePatchType)
				    fgetc(f);

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
obj_ReadRGB0(FILE * pObjfile)
{
	struct sSection *pFirstSection;
	SLONG nNumberOfSymbols, nNumberOfSections, i;

	nNumberOfSymbols = readlong(pObjfile);
	nNumberOfSections = readlong(pObjfile);

	/* First comes the symbols */

	if (nNumberOfSymbols) {
		tSymbols = malloc(nNumberOfSymbols * sizeof(struct sSymbol *));
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

		pNewSection = obj_ReadRGB0Section(pObjfile);
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
 * RGB1 object reader routines
 *
 */

struct sSection *
obj_ReadRGBSection(FILE * f, enum ObjectFileContents contents)
{
	struct sSection *pSection;

	pSection = AllocSection();

	if (contents & CONTAINS_SECTION_NAME) {
		readasciiz(&pSection->pzName, f);
	} else {
		pSection->pzName = "";
	}

	pSection->nByteSize = readlong(f);
	pSection->Type = (enum eSectionType) fgetc(f);
	pSection->nOrg = readlong(f);
	pSection->nBank = readlong(f);
	
	if (contents & CONTAINS_SECTION_ALIGNMENT) {
		pSection->nAlign = readlong(f);
	} else {
		pSection->nAlign = 1;
	}

	/* does the user want the -s mode? */

	if ((options & OPT_SMALL) && (pSection->Type == SECT_ROMX)) {
		pSection->Type = SECT_ROM0;
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
obj_ReadRGB(FILE * pObjfile, enum ObjectFileContents contents)
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

		pNewSection = obj_ReadRGBSection(pObjfile, contents);
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
		case '0':
			obj_ReadRGB0(pObjfile);
			break;
		case '1':
		case '2':
			//V2 is really the same but the are new patch types
			obj_ReadRGB(pObjfile, 0);
			break;
		case '3': // V3 is very similiar, but contains section names and byte alignment
		case '4': // V4 supports OAM sections, but is otherwise identical
			obj_ReadRGB(pObjfile, CONTAINS_SECTION_NAME | CONTAINS_SECTION_ALIGNMENT);
			break;
		default:
			errx(1, "'%s' is an unsupported version", tzObjectfile);
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
