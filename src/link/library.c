#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/err.h"
#include "types.h"
#include "link/mylink.h"
#include "link/main.h"

static BBOOL
symboldefined(char *name)
{
	struct sSection *pSect;

	pSect = pSections;

	while (pSect) {
		int i;

		for (i = 0; i < pSect->nNumberOfSymbols; i += 1) {
			if ((pSect->tSymbols[i]->Type == SYM_EXPORT)
			    || ((pSect->tSymbols[i]->Type == SYM_LOCAL)
				&& (pSect == pSect->tSymbols[i]->pSection))) {
				if (strcmp(pSect->tSymbols[i]->pzName, name) ==
				    0)
					return (1);
			}
		}
		pSect = pSect->pNext;
	}
	return (0);
}

static BBOOL
addmodulecontaining(char *name)
{
	struct sSection **ppLSect;

	ppLSect = &pLibSections;

	while (*ppLSect) {
		int i;

		for (i = 0; i < (*ppLSect)->nNumberOfSymbols; i += 1) {
			if (((*ppLSect)->tSymbols[i]->Type == SYM_EXPORT)
			    || (((*ppLSect)->tSymbols[i]->Type == SYM_LOCAL)
				&& ((*ppLSect) ==
				    (*ppLSect)->tSymbols[i]->pSection))) {
				if (strcmp
				    ((*ppLSect)->tSymbols[i]->pzName,
					name) == 0) {
					struct sSection **ppSect;
					ppSect = &pSections;
					while (*ppSect)
						ppSect = &((*ppSect)->pNext);

					*ppSect = *ppLSect;
					*ppLSect = (*ppLSect)->pNext;
					(*ppSect)->pNext = NULL;
					return (1);
				}
			}
		}
		ppLSect = &((*ppLSect)->pNext);
	}
	return (0);
}

void
AddNeededModules(void)
{
	struct sSection *pSect;

	if ((options & OPT_SMART_C_LINK) == 0) {
		struct sSection **ppLSect;

		ppLSect = &pLibSections;

		while (*ppLSect) {
			struct sSection **ppSect;
			ppSect = &pSections;
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
		} else
			printf("Smart linking with symbol '%s'\n",
			    smartlinkstartsymbol);
	}
	pSect = pSections;

	while (pSect) {
		int i;

		for (i = 0; i < pSect->nNumberOfSymbols; i += 1) {
			if ((pSect->tSymbols[i]->Type == SYM_IMPORT)
			    || (pSect->tSymbols[i]->Type == SYM_LOCAL)) {
				if (!symboldefined(pSect->tSymbols[i]->pzName)) {
					addmodulecontaining(pSect->tSymbols[i]->
					    pzName);
				}
			}
		}
		pSect = pSect->pNext;
	}
}
