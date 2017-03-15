#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link/mylink.h"
#include "link/mapfile.h"
#include "link/main.h"
#include "link/assign.h"

char *tzOutname;
char *tzOverlayname = NULL;

SLONG MaxOverlayBank;

void 
writehome(FILE * f, FILE * f_overlay)
{
	struct sSection *pSect;
	UBYTE *mem;

	mem = malloc(MaxAvail[BANK_ROM0]);
	if (!mem)
		return;
	
	if (f_overlay != NULL) {
		fseek(f_overlay, 0L, SEEK_SET);
		fread(mem, 1, MaxAvail[BANK_ROM0], f_overlay);
	} else {
		memset(mem, fillchar, MaxAvail[BANK_ROM0]);
	}
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
writebank(FILE * f, FILE * f_overlay, SLONG bank)
{
	struct sSection *pSect;
	UBYTE *mem;

	mem = malloc(MaxAvail[bank]);
	if (!mem)
		return;

	if (f_overlay != NULL && bank <= MaxOverlayBank) {
		fseek(f_overlay, bank*0x4000, SEEK_SET);
		fread(mem, 1, MaxAvail[bank], f_overlay);
	} else {
		memset(mem, fillchar, MaxAvail[bank]);
	}
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
out_SetOverlayname(char *tzOverlayfile)
{
	tzOverlayname = tzOverlayfile;
}


void 
Output(void)
{
	SLONG i;
	FILE *f;
	FILE *f_overlay = NULL;
	

	if ((f = fopen(tzOutname, "wb"))) {
		if (tzOverlayname) {
			f_overlay = fopen(tzOverlayname, "rb");
			if (!f_overlay) {
				fprintf(stderr, "Failed to open overlay file %s\n", tzOverlayname);
				exit(1);
			}
			fseek(f_overlay, 0, SEEK_END);
			if (ftell(f_overlay) % 0x4000 != 0) {
				fprintf(stderr, "Overlay file must be aligned to 0x4000 bytes\n");
				exit(1);
			}
			MaxOverlayBank = (ftell(f_overlay) / 0x4000) - 1;
			if (MaxOverlayBank < 1) {
				fprintf(stderr, "Overlay file be at least 0x8000 bytes\n");
				exit(1);
			}
			if (MaxOverlayBank > MaxBankUsed) {
				MaxBankUsed = MaxOverlayBank;
			}
		}
		
		writehome(f, f_overlay);
		for (i = 1; i <= MaxBankUsed; i += 1)
			writebank(f, f_overlay, i);

		fclose(f);
		
		if (tzOverlayname) {
			fclose(f_overlay);
		}
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
