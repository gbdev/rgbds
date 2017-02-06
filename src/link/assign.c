#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "extern/err.h"
#include "link/mylink.h"
#include "link/main.h"
#include "link/symbol.h"
#include "link/assign.h"

struct sFreeArea {
	SLONG nOrg;
	SLONG nSize;
	struct sFreeArea *pPrev, *pNext;
};

struct sSectionAttributes {
	const char *name;
	SLONG bank;
	SLONG offset; // bank + offset = bank originally stored in a section struct
	SLONG minBank;
	SLONG bankCount;
};

struct sFreeArea *BankFree[MAXBANKS];
SLONG MaxAvail[MAXBANKS];
SLONG MaxBankUsed;
SLONG MaxWBankUsed;
SLONG MaxSBankUsed;
SLONG MaxVBankUsed;

const enum eSectionType SECT_MIN = SECT_WRAM0;
const enum eSectionType SECT_MAX = SECT_SRAM;
const struct sSectionAttributes SECT_ATTRIBUTES[] = {
	{"WRAM0", BANK_WRAM0, 0, 0, BANK_COUNT_WRAM0},
	{"VRAM",  BANK_VRAM,  0, 0, BANK_COUNT_VRAM},
	{"ROMX",  BANK_ROMX, -1, 1, BANK_COUNT_ROMX},
	{"ROM0",  BANK_ROM0,  0, 0, BANK_COUNT_ROM0},
	{"HRAM",  BANK_HRAM,  0, 0, BANK_COUNT_HRAM},
	{"WRAMX", BANK_WRAMX, 0, 0, BANK_COUNT_WRAMX},
	{"SRAM",  BANK_SRAM,  0, 0, BANK_COUNT_SRAM}
};

#define DOMAXBANK(x, y) {switch (x) { \
	case SECT_ROMX: DOMAXRBANK(y); break; \
	case SECT_WRAMX: DOMAXWBANK(y); break; \
	case SECT_SRAM: DOMAXSBANK(y); break; \
	case SECT_VRAM: DOMAXVBANK(y); break; \
	default: errx(1, "DOMAXBANK used with invalid parameters"); break; }}
#define DOMAXRBANK(x)	{if( (x)>MaxBankUsed ) MaxBankUsed=(x);}
#define DOMAXWBANK(x)	{if( (x)>MaxWBankUsed ) MaxWBankUsed=(x);}
#define DOMAXSBANK(x)	{if( (x)>MaxSBankUsed ) MaxSBankUsed=(x);}
#define DOMAXVBANK(x)	{if( (x)>MaxVBankUsed ) MaxVBankUsed=(x);}

void
ensureSectionTypeIsValid(enum eSectionType type)
{
	if (type < SECT_MIN || type > SECT_MAX) {
		errx(1, "(INTERNAL) Invalid section type found.");
	}
}

SLONG
area_Avail(SLONG bank)
{
	SLONG r;
	struct sFreeArea *pArea;

	r = 0;
	pArea = BankFree[bank];

	while (pArea) {
		r += pArea->nSize;
		pArea = pArea->pNext;
	}

	return (r);
}

SLONG
area_AllocAbs(struct sFreeArea ** ppArea, SLONG org, SLONG size)
{
	struct sFreeArea *pArea;

	pArea = *ppArea;
	while (pArea) {
		if (org >= pArea->nOrg
		    && (org + size - 1) <= (pArea->nOrg + pArea->nSize - 1)) {
			if (org == pArea->nOrg) {
				pArea->nOrg += size;
				pArea->nSize -= size;
				return 0;
			} else {
				if ((org + size - 1) ==
				    (pArea->nOrg + pArea->nSize - 1)) {
					pArea->nSize -= size;
					return 0;
				} else {
					struct sFreeArea *pNewArea;

					if ((pNewArea =
						malloc(sizeof(struct sFreeArea)))
					    != NULL) {
						*pNewArea = *pArea;
						pNewArea->pPrev = pArea;
						pArea->pNext = pNewArea;
						pArea->nSize =
						    org - pArea->nOrg;
						pNewArea->nOrg = org + size;
						pNewArea->nSize -=
						    size + pArea->nSize;

						return 0;
					} else {
						err(1, NULL);
					}
				}
			}
		}
		ppArea = &(pArea->pNext);
		pArea = *ppArea;
	}

	return -1;
}

SLONG
area_AllocAbsAnyBank(SLONG org, SLONG size, enum eSectionType type)
{
	ensureSectionTypeIsValid(type);

	SLONG startBank = SECT_ATTRIBUTES[type].bank;
	SLONG bankCount = SECT_ATTRIBUTES[type].bankCount;

	for (int i = 0; i < bankCount; i++) {
		if (area_AllocAbs(&BankFree[startBank + i], org, size) != -1) {
			return startBank + i;
		}
	}

	return -1;
}

SLONG
area_Alloc(struct sFreeArea ** ppArea, SLONG size)
{
	struct sFreeArea *pArea;

	pArea = *ppArea;
	while (pArea) {
		if (size <= pArea->nSize) {
			SLONG r;

			r = pArea->nOrg;
			pArea->nOrg += size;
			pArea->nSize -= size;

			return (r);
		}
		ppArea = &(pArea->pNext);
		pArea = *ppArea;
	}

	return (-1);
}

SLONG
area_AllocAnyBank(SLONG size, enum eSectionType type) {
	ensureSectionTypeIsValid(type);

	SLONG startBank = SECT_ATTRIBUTES[type].bank;
	SLONG bankCount = SECT_ATTRIBUTES[type].bankCount;

	for (int i = 0; i < bankCount; i++) {
		SLONG org = area_Alloc(&BankFree[startBank + i], size);
		if (org != -1) {
			return ((startBank + i) << 16) | org;
		}
	}

	return -1;
}

struct sSection *
FindLargestSection(enum eSectionType type)
{
	struct sSection *pSection, *r = NULL;
	SLONG nLargest = 0;

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0 && pSection->Type == type) {
			if (pSection->nByteSize > nLargest) {
				nLargest = pSection->nByteSize;
				r = pSection;
			}
		}
		pSection = pSection->pNext;
	}
	return r;
}

void
AssignBankedSections(enum eSectionType type)
{
	ensureSectionTypeIsValid(type);

	struct sSection *pSection;

	while ((pSection = FindLargestSection(type))) {
		SLONG org;

		if ((org = area_AllocAnyBank(pSection->nByteSize, type)) != -1) {
			pSection->nOrg = org & 0xFFFF;
			pSection->nBank = org >> 16;
			pSection->oAssigned = 1;
			DOMAXBANK(pSection->Type, pSection->nBank);
		} else {
			errx(1, "Unable to place %s section anywhere",
				 SECT_ATTRIBUTES[type].name);
		}
	}
}

bool
VerifyAndSetBank(struct sSection *pSection)
{
	ensureSectionTypeIsValid(pSection->Type);

	if (pSection->nBank >= SECT_ATTRIBUTES[pSection->Type].minBank
		&& pSection->nBank < SECT_ATTRIBUTES[pSection->Type].minBank + SECT_ATTRIBUTES[pSection->Type].bankCount) {
		pSection->nBank += SECT_ATTRIBUTES[pSection->Type].bank + SECT_ATTRIBUTES[pSection->Type].offset;
		return true;

	} else {
		return false;
	}
}

void
AssignSections(void)
{
	SLONG i;
	struct sSection *pSection;

	MaxBankUsed = 0;

	/*
	 * Initialize the memory areas
	 *
	 */

	for (i = 0; i < MAXBANKS; i += 1) {
		BankFree[i] = malloc(sizeof *BankFree[i]);

		if (!BankFree[i]) {
			err(1, NULL);
		}

		if (i == BANK_ROM0) {
			/* ROM0 bank */
			BankFree[i]->nOrg = 0x0000;
			if (options & OPT_SMALL) {
				BankFree[i]->nSize = 0x8000;
			} else {
				BankFree[i]->nSize = 0x4000;
			}
		} else if (i >= BANK_ROMX && i < BANK_ROMX + BANK_COUNT_ROMX) {
			/* Swappable ROM bank */
			BankFree[i]->nOrg = 0x4000;
			/*
			 * Now, this shouldn't really be necessary... but for
			 * good measure we'll do it anyway.
			 */
			if (options & OPT_SMALL) {
				BankFree[i]->nSize = 0;
			} else {
				BankFree[i]->nSize = 0x4000;
			}
		} else if (i == BANK_WRAM0) {
			/* WRAM */
			BankFree[i]->nOrg = 0xC000;
			if (options & OPT_CONTWRAM) {
				BankFree[i]->nSize = 0x2000;
			} else {
				BankFree[i]->nSize = 0x1000;
			}
		} else if (i >= BANK_SRAM && i < BANK_SRAM + BANK_COUNT_SRAM) {
			/* Swappable SRAM bank */
			BankFree[i]->nOrg = 0xA000;
			BankFree[i]->nSize = 0x2000;
		} else if (i >= BANK_WRAMX && i < BANK_WRAMX + BANK_COUNT_WRAMX) {
			/* Swappable WRAM bank */
			BankFree[i]->nOrg = 0xD000;
			BankFree[i]->nSize = 0x1000;
		} else if (i >= BANK_VRAM && i < BANK_VRAM + BANK_COUNT_VRAM) {
			/* Swappable VRAM bank */
			BankFree[i]->nOrg = 0x8000;
			BankFree[i]->nSize = 0x2000;
		} else if (i == BANK_HRAM) {
			/* HRAM */
			BankFree[i]->nOrg = 0xFF80;
			BankFree[i]->nSize = 0x007F;
		} else {
			errx(1, "(INTERNAL) Unknown bank type!");
		}

		MaxAvail[i] = BankFree[i]->nSize;
		BankFree[i]->pPrev = NULL;
		BankFree[i]->pNext = NULL;
	}

	/*
	 * First, let's assign all the fixed sections...
	 * And all because of that Jens Restemeier character ;)
	 *
	 */

	pSection = pSections;
	while (pSection) {
		if ((pSection->nOrg != -1 || pSection->nBank != -1)
		    && pSection->oAssigned == 0) {
			/* User wants to have a say... */

			if (pSection->Type == SECT_WRAMX && options & OPT_CONTWRAM) {
				errx(1, "WRAMX not compatible with -w!");
			}

			switch (pSection->Type) {
			case SECT_WRAM0:
			case SECT_HRAM:
			case SECT_ROM0:
				pSection->nBank = SECT_ATTRIBUTES[pSection->Type].bank;
				if (area_AllocAbs(&BankFree[pSection->nBank], pSection->nOrg,
					 pSection->nByteSize) == -1) {
					errx(1, "Unable to load fixed %s section at $%lX",
						 SECT_ATTRIBUTES[pSection->Type].name,
						 pSection->nOrg);
				}
				pSection->oAssigned = 1;
				break;

			case SECT_SRAM:
			case SECT_WRAMX:
			case SECT_VRAM:
			case SECT_ROMX:
				if (pSection->nBank != -1 && pSection->nOrg != -1) {
					if (VerifyAndSetBank(pSection) &&
						area_AllocAbs(&BankFree[pSection->nBank], pSection->nOrg, pSection->nByteSize) != -1) {
						DOMAXBANK(pSection->Type, pSection->nBank);
						pSection->oAssigned = 1;
					} else {
						errx(1,
							 "Unable to load fixed %s section at $%lX in bank $%02lX", SECT_ATTRIBUTES[pSection->Type].name, pSection->nOrg, pSection->nBank);
					}
				}
				break;
			}
		}
		pSection = pSection->pNext;
	}

	/*
	 * Next, let's assign all the bankfixed ONLY ROMX sections...
	 *
	 */

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0
			&& pSection->nOrg == -1 && pSection->nBank != -1) {
			switch (pSection->Type) {
			case SECT_ROMX:
			case SECT_SRAM:
			case SECT_VRAM:
			case SECT_WRAMX:
				if (VerifyAndSetBank(pSection) &&
					(pSection->nOrg = area_Alloc(&BankFree[pSection->nBank], pSection->nByteSize)) != -1) {
					pSection->oAssigned = 1;
					DOMAXBANK(pSection->Type, pSection->nBank);
				} else {
					errx(1, "Unable to load fixed %s section into bank $%02lX",
						 SECT_ATTRIBUTES[pSection->Type].name, pSection->nBank);
				}
				break;

			default: // Handle other sections later
				break;
			}
		}

		pSection = pSection->pNext;
	}

	/*
	 * Now, let's assign all the floating bank but fixed ROMX sections...
	 *
	 */

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0
			&& pSection->nOrg != -1 && pSection->nBank == -1) {
			switch (pSection->Type) {
			case SECT_ROMX:
			case SECT_VRAM:
			case SECT_SRAM:
			case SECT_WRAMX:
				if ((pSection->nBank =
					area_AllocAbsAnyBank(pSection->nOrg, pSection->nByteSize,
						pSection->Type)) == -1) {
					errx(1, "Unable to load fixed %s section at $%lX into any bank",
						 SECT_ATTRIBUTES[pSection->Type].name, pSection->nOrg);
				}
				pSection->oAssigned = 1;
				DOMAXBANK(pSection->Type, pSection->nBank);
				break;

			default: // Handle other sections later
				break;
			}
		}

		pSection = pSection->pNext;
	}

	/*
	 * OK, all that nasty stuff is done so let's assign all the other
	 * sections
	 *
	 */

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0) {
			switch (pSection->Type) {
			case SECT_WRAM0:
			case SECT_HRAM:
			case SECT_ROM0:
				pSection->nBank = SECT_ATTRIBUTES[pSection->Type].bank;
				if ((pSection->nOrg =
					area_Alloc(&BankFree[pSection->nBank],
					    pSection->nByteSize)) == -1) {
					errx(1, "%s section too large", SECT_ATTRIBUTES[pSection->Type].name);
				}
				pSection->oAssigned = 1;
				break;

			case SECT_SRAM:
			case SECT_VRAM:
			case SECT_WRAMX:
			case SECT_ROMX:
				break;

			default:
				errx(1, "(INTERNAL) Unknown section type!");
				break;
			}
		}
		pSection = pSection->pNext;
	}

	AssignBankedSections(SECT_ROMX);
	AssignBankedSections(SECT_VRAM);
	AssignBankedSections(SECT_WRAMX);
	AssignBankedSections(SECT_SRAM);
}

void
CreateSymbolTable(void)
{
	struct sSection *pSect;

	sym_Init();

	pSect = pSections;

	while (pSect) {
		SLONG i;

		i = pSect->nNumberOfSymbols;

		while (i--) {
			if ((pSect->tSymbols[i]->Type == SYM_EXPORT) &&
			    ((pSect->tSymbols[i]->pSection == pSect) ||
				(pSect->tSymbols[i]->pSection == NULL))) {
				if (pSect->tSymbols[i]->pSection == NULL)
					sym_CreateSymbol(pSect->tSymbols[i]->
					    pzName,
					    pSect->tSymbols[i]->
					    nOffset, -1);
				else
					sym_CreateSymbol(pSect->tSymbols[i]->
					    pzName,
					    pSect->nOrg +
					    pSect->tSymbols[i]->
					    nOffset, pSect->nBank);
			}
		}
		pSect = pSect->pNext;
	}
}
