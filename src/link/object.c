/*
 * Here we have the routines that read an objectfile
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
readasciiz(char *s, FILE * f)
{
	SLONG r = 0;

	while (((*s++) = fgetc(f)) != 0)
		r += 1;

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
		fprintf(stderr, "Out of memory!\n");
		exit(1);
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
	char s[256];
	struct sSymbol *pSym;

	pSym = malloc(sizeof *pSym);
	if (!pSym) {
		fprintf(stderr, "Out of memory!\n");
		exit(1);
	}

	readasciiz(s, f);
	pSym->pzName = malloc(strlen(s) + 1);
	if (!pSym->pzName) {
		fprintf(stderr, "Out of memory!\n");
		exit(1);
	}

	strcpy(pSym->pzName, s);
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

	pSection->nByteSize = readlong(f);
	pSection->Type = (enum eSectionType) fgetc(f);
	pSection->nOrg = -1;
	pSection->nBank = -1;

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
				fprintf(stderr, "Out of memory!\n");
			}

			SLONG nNumberOfPatches;
			struct sPatch **ppPatch, *pPatch;
			char s[256];

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
					fprintf(stderr, "Out of memory!\n");
				}

				*ppPatch = pPatch;
				readasciiz(s, f);

				pPatch->pzFilename = malloc(strlen(s) + 1);
				if (!pPatch->pzFilename) {
					fprintf(stderr, "Out of memory!\n");
				}

				strcpy(pPatch->pzFilename, s);

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
						fprintf(stderr, "Out of memory!\n");
						exit(1);
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
			fprintf(stderr, "Out of memory!\n");
			exit(1);
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
obj_ReadRGB1Section(FILE * f)
{
	struct sSection *pSection;

	pSection = AllocSection();

	pSection->nByteSize = readlong(f);
	pSection->Type = (enum eSectionType) fgetc(f);
	/*
	 * And because of THIS new feature I'll have to rewrite loads and
	 * loads of stuff... oh well it needed to be done anyway
	 *
	 */
	pSection->nOrg = readlong(f);
	pSection->nBank = readlong(f);

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
				fprintf(stderr, "Out of memory!\n");
				exit(1);
			}

			SLONG nNumberOfPatches;
			struct sPatch **ppPatch, *pPatch;
			char s[256];

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
					fprintf(stderr, "Out of memory!\n");
				}

				*ppPatch = pPatch;
				readasciiz(s, f);
				pPatch->pzFilename = malloc(strlen(s) + 1);
				if (!pPatch->pzFilename) {
					fprintf(stderr, "Out of memory!\n");
				}

				strcpy(pPatch->pzFilename, s);
				pPatch->nLineNo = readlong(f);
				pPatch->nOffset = readlong(f);
				pPatch->Type = (enum ePatchType) fgetc(f);
				if ((pPatch->nRPNSize = readlong(f)) > 0) {
					pPatch->pRPN = malloc(pPatch->nRPNSize);
					if (!pPatch->pRPN) {
						fprintf(stderr, "Out of memory!\n");
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
obj_ReadRGB1(FILE * pObjfile)
{
	struct sSection *pFirstSection;
	SLONG nNumberOfSymbols, nNumberOfSections, i;

	nNumberOfSymbols = readlong(pObjfile);
	nNumberOfSections = readlong(pObjfile);

	/* First comes the symbols */

	if (nNumberOfSymbols) {
		tSymbols = malloc(nNumberOfSymbols * sizeof *tSymbols);
		if (!tSymbols) {
			fprintf(stderr, "Out of memory!\n");
		}

		for (i = 0; i < nNumberOfSymbols; i += 1)
			tSymbols[i] = obj_ReadSymbol(pObjfile);
	} else
		tSymbols = (struct sSymbol **) & dummymem;

	/* Next we have the sections */

	pFirstSection = NULL;
	while (nNumberOfSections--) {
		struct sSection *pNewSection;

		pNewSection = obj_ReadRGB1Section(pObjfile);
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
			    obj_ReadRGB1(pObjfile);
			break;
		default:
			fprintf(stderr, "'%s' is an unsupported version",
			    tzObjectfile);
			exit(1);
			break;
		}
	} else {
		fprintf(stderr, "'%s' is not a valid object\n", tzObjectfile);
		exit(1);
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
		fprintf(stderr, "Unable to open object '%s': ",
		    tzObjectfile);
		perror(NULL);
		exit(1);
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
		char name[256];

		size -= readasciiz(name, f);
		readword(f);
		size -= 2;
		readword(f);
		size -= 2;
		size -= readlong(f);
		size -= 4;
		obj_ReadOpenFile(f, name);
	}
}

void 
lib_Readfile(char *tzLibfile)
{
	FILE *pObjfile;

	oReadLib = 1;

	pObjfile = fopen(tzLibfile, "rb");
	if (pObjfile == NULL) {
		fprintf(stderr, "Unable to open object '%s': ", tzLibfile);
		perror(NULL);
		exit(1);
	}
	if (!pObjfile) {
		fprintf(stderr, "Unable to open '%s'\n", tzLibfile);
		exit(1);
	}
	char tzHeader[5];

	fread(tzHeader, sizeof(char), 4, pObjfile);
	tzHeader[4] = 0;
	if (strcmp(tzHeader, "XLB0") == 0)
		lib_ReadXLB0(pObjfile);
	else {
		fprintf(stderr, "'%s' is an invalid library\n",
		    tzLibfile);
		exit(1);
	}
	fclose(pObjfile);
}
