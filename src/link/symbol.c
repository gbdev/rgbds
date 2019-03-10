/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"

#include "link/main.h"
#include "link/patch.h"
#include "link/mylink.h"

#include "types.h"

#define HASHSIZE 73

struct ISymbol {
	char *pzName;
	int32_t nValue;
	int32_t nBank; /* -1 = constant */
	 /* Object file where the symbol was defined. */
	char tzObjFileName[_MAX_PATH + 1];
	/* Source file where the symbol was defined. */
	char tzFileName[_MAX_PATH + 1];
	/* Line where the symbol was defined. */
	uint32_t nFileLine;
	struct ISymbol *pNext;
};

struct ISymbol *tHash[HASHSIZE];

int32_t calchash(char *s)
{
	int32_t r = 0;

	while (*s)
		r += *s++;

	return r % HASHSIZE;
}

void sym_Init(void)
{
	int32_t i;

	for (i = 0; i < HASHSIZE; i += 1)
		tHash[i] = NULL;
}

int32_t sym_GetValue(struct sPatch *pPatch, char *tzName)
{
	if (strcmp(tzName, "@") == 0)
		return nPC;

	struct ISymbol **ppSym;

	ppSym = &(tHash[calchash(tzName)]);
	while (*ppSym) {
		if (strcmp(tzName, (*ppSym)->pzName))
			ppSym = &((*ppSym)->pNext);
		else
			return ((*ppSym)->nValue);
	}

	errx(1,
	     "%s(%ld) : Unknown symbol '%s'",
	     pPatch->pzFilename, pPatch->nLineNo,
	     tzName);
}

int32_t sym_GetBank(struct sPatch *pPatch, char *tzName)
{
	struct ISymbol **ppSym;

	ppSym = &(tHash[calchash(tzName)]);
	while (*ppSym) {
		if (strcmp(tzName, (*ppSym)->pzName))
			ppSym = &((*ppSym)->pNext);
		else
			return ((*ppSym)->nBank);
	}

	errx(1,
	     "%s(%ld) : Unknown symbol '%s'",
	     pPatch->pzFilename, pPatch->nLineNo,
	     tzName);
}

void sym_CreateSymbol(char *tzName, int32_t nValue, int32_t nBank,
		      char *tzObjFileName, char *tzFileName, uint32_t nFileLine)
{
	if (strcmp(tzName, "@") == 0)
		return;

	struct ISymbol **ppSym;

	ppSym = &(tHash[calchash(tzName)]);

	while (*ppSym) {
		if (strcmp(tzName, (*ppSym)->pzName)) {
			ppSym = &((*ppSym)->pNext);
		} else {
			if (nBank == -1)
				return;

			errx(1, "'%s' in both %s : %s(%d) and %s : %s(%d)",
			     tzName, tzObjFileName, tzFileName, nFileLine,
			     (*ppSym)->tzObjFileName,
			     (*ppSym)->tzFileName, (*ppSym)->nFileLine);
		}
	}

	*ppSym = malloc(sizeof **ppSym);

	if (*ppSym != NULL) {
		(*ppSym)->pzName = malloc(strlen(tzName) + 1);

		if ((*ppSym)->pzName != NULL) {
			strcpy((*ppSym)->pzName, tzName);
			(*ppSym)->nValue = nValue;
			(*ppSym)->nBank = nBank;
			(*ppSym)->pNext = NULL;
			strncpy((*ppSym)->tzObjFileName, tzObjFileName,
				sizeof((*ppSym)->tzObjFileName));
			strncpy((*ppSym)->tzFileName, tzFileName,
				sizeof((*ppSym)->tzFileName));
			(*ppSym)->nFileLine = nFileLine;
		}
	}
}
