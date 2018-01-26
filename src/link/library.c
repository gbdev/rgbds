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

#include "link/mylink.h"
#include "link/main.h"

static uint8_t symboldefined(char *name)
{
	const struct sSection *pSect;

	pSect = pSections;

	while (pSect) {
		int32_t i;

		for (i = 0; i < pSect->nNumberOfSymbols; i += 1) {
			const struct sSymbol *tSymbol = pSect->tSymbols[i];

			if ((tSymbol->Type == SYM_EXPORT)
			    || ((tSymbol->Type == SYM_LOCAL)
				&& (pSect == tSymbol->pSection))) {

				if (strcmp(tSymbol->pzName, name) == 0)
					return 1;
			}
		}
		pSect = pSect->pNext;
	}
	return 0;
}

static uint8_t addmodulecontaining(char *name)
{
	struct sSection **ppLSect = &pLibSections;

	while (*ppLSect) {
		int32_t i;

		for (i = 0; i < (*ppLSect)->nNumberOfSymbols; i += 1) {
			const struct sSymbol *tSymbol = (*ppLSect)->tSymbols[i];

			if ((tSymbol->Type == SYM_EXPORT)
			    || ((tSymbol->Type == SYM_LOCAL)
				&& ((*ppLSect) == tSymbol->pSection))) {

				if (strcmp(tSymbol->pzName, name) == 0) {
					struct sSection **ppSect = &pSections;

					while (*ppSect)
						ppSect = &((*ppSect)->pNext);

					*ppSect = *ppLSect;
					*ppLSect = (*ppLSect)->pNext;
					(*ppSect)->pNext = NULL;
					return 1;
				}
			}
		}
		ppLSect = &((*ppLSect)->pNext);
	}
	return 0;
}

void AddNeededModules(void)
{
	struct sSection *pSect;

	if ((options & OPT_SMART_C_LINK) == 0) {
		struct sSection **ppLSect;

		ppLSect = &pLibSections;

		while (*ppLSect) {
			struct sSection **ppSect = &pSections;

			while (*ppSect)
				ppSect = &((*ppSect)->pNext);

			*ppSect = *ppLSect;
			*ppLSect = (*ppLSect)->pNext;
			(*ppSect)->pNext = NULL;

			/* ppLSect=&((*ppLSect)->pNext); */
		}
		return;
	}
	if (options & OPT_SMART_C_LINK) {
		if (!addmodulecontaining(smartlinkstartsymbol)) {
			errx(1, "Can't find start symbol '%s'",
			     smartlinkstartsymbol);
		} else {
			printf("Smart linking with symbol '%s'\n",
			       smartlinkstartsymbol);
		}
	}
	pSect = pSections;

	while (pSect) {
		int32_t i;

		for (i = 0; i < pSect->nNumberOfSymbols; i += 1) {
			if ((pSect->tSymbols[i]->Type == SYM_IMPORT)
			    || (pSect->tSymbols[i]->Type == SYM_LOCAL)) {
				if (!symboldefined(pSect->tSymbols[i]->pzName))
					addmodulecontaining(pSect->tSymbols[i]->pzName);
			}
		}
		pSect = pSect->pNext;
	}
}
