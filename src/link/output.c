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
#include "link/mapfile.h"
#include "link/main.h"
#include "link/assign.h"

#include "safelibc.h"

char *tzOutname;
char *tzOverlayname;

int32_t MaxOverlayBank;

void writehome(FILE *f, FILE *f_overlay)
{
	const struct sSection *pSect;
	uint8_t *mem;

	mem = zmalloc(MaxAvail[BANK_INDEX_ROM0]);

	if (f_overlay != NULL) {
		zfseek(f_overlay, 0L, SEEK_SET);
		zfread(mem, 1, MaxAvail[BANK_INDEX_ROM0], f_overlay);
	} else {
		memset(mem, fillchar, MaxAvail[BANK_INDEX_ROM0]);
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

	zfwrite(mem, 1, MaxAvail[BANK_INDEX_ROM0], f);
	zfree(mem);
}

void writebank(FILE *f, FILE *f_overlay, int32_t bank)
{
	const struct sSection *pSect;
	uint8_t *mem;

	mem = zmalloc(MaxAvail[bank]);

	if (f_overlay != NULL && bank <= MaxOverlayBank) {
		zfseek(f_overlay, bank * 0x4000, SEEK_SET);
		zfread(mem, 1, MaxAvail[bank], f_overlay);
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

	zfwrite(mem, 1, MaxAvail[bank], f);
	zfree(mem);
}

void out_Setname(char *tzOutputfile)
{
	tzOutname = tzOutputfile;
}

void out_SetOverlayname(char *tzOverlayfile)
{
	tzOverlayname = tzOverlayfile;
}

void Output(void)
{
	int32_t i;
	FILE *f;
	FILE *f_overlay = NULL;

	/*
	 * Apply overlay
	 */

	f = zfopen(tzOutname, "wb");

	if (tzOverlayname) {
		f_overlay = zfopen(tzOverlayname, "rb");

		zfseek(f_overlay, 0, SEEK_END);

		if (zftell(f_overlay) % 0x4000 != 0)
			errx(1, "Overlay file must be aligned to 0x4000 bytes.");

		MaxOverlayBank = (zftell(f_overlay) / 0x4000) - 1;

		if (MaxOverlayBank < 1)
			errx(1, "Overlay file must be at least 0x8000 bytes.");

		if (MaxOverlayBank > MaxBankUsed)
			MaxBankUsed = MaxOverlayBank;

		/* Write data to ROM */
		writehome(f, f_overlay);
		for (i = 1; i <= MaxBankUsed; i += 1)
			writebank(f, f_overlay, i);

		zfclose(f_overlay);
	} else {
		/* Write data to ROM */
		writehome(f, NULL);
		for (i = 1; i <= MaxBankUsed; i += 1)
			writebank(f, NULL, i);
	}

	zfclose(f);

	/*
	 * Write map and sym files
	 */

	for (i = BANK_INDEX_WRAM0; i < BANK_INDEX_MAX; i++) {
		const struct sSection *pSect;

		MapfileInitBank(i);
		pSect = pSections;
		while (pSect) {
			if (pSect->nBank == i)
				MapfileWriteSection(pSect);
			pSect = pSect->pNext;
		}
		MapfileCloseBank(area_Avail(i));
	}
}
