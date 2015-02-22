#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link/mylink.h"
#include "link/mapfile.h"
#include "link/main.h"
#include "link/assign.h"

char *tzOutname;

void 
writehome(FILE * f)
{
	struct sSection *pSect;
	UBYTE *mem;

	mem = malloc(MaxAvail[BANK_ROM0]);
	if (!mem)
		return;

	memset(mem, fillchar, MaxAvail[BANK_ROM0]);
	MapfileInitBank(0);

	pSect = pSections;
	while (pSect) {
		if (pSect->Type == SECT_ROM0) {
			memcpy(mem + pSect->nOrg, pSect->pData,
			    pSect->nByteSize);
			MapfileWriteSection(pSect);
		}
		pSect = pSect->pNext;
	}

	MapfileCloseBank(area_Avail(0));

	fwrite(mem, 1, MaxAvail[BANK_ROM0], f);
	free(mem);
}

void 
writebank(FILE * f, SLONG bank)
{
	struct sSection *pSect;
	UBYTE *mem;

	mem = malloc(MaxAvail[bank]);
	if (!mem)
		return;

	memset(mem, fillchar, MaxAvail[bank]);
	MapfileInitBank(bank);

	pSect = pSections;
	while (pSect) {
		if (pSect->Type == SECT_ROMX && pSect->nBank == bank) {
			memcpy(mem + pSect->nOrg - 0x4000, pSect->pData,
			    pSect->nByteSize);
			MapfileWriteSection(pSect);
		}
		pSect = pSect->pNext;
	}

	MapfileCloseBank(area_Avail(bank));

	fwrite(mem, 1, MaxAvail[bank], f);
	free(mem);
}

void 
out_Setname(char *tzOutputfile)
{
	tzOutname = tzOutputfile;
}

void 
Output(void)
{
	SLONG i;
	FILE *f;

	if ((f = fopen(tzOutname, "wb"))) {
		writehome(f);
		for (i = 1; i <= MaxBankUsed; i += 1)
			writebank(f, i);

		fclose(f);
	}
	for (i = BANK_WRAM0; i < MAXBANKS; i++) {
		struct sSection *pSect;
		MapfileInitBank(i);
		pSect = pSections;
		while (pSect) {
			if (pSect->nBank == i) {
				MapfileWriteSection(pSect);
			}
			pSect = pSect->pNext;
		}
		MapfileCloseBank(area_Avail(i));
	}
}
