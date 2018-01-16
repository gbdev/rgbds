#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "extern/err.h"

#include "link/assign.h"
#include "link/mylink.h"
#include "link/main.h"
#include "link/script.h"
#include "link/symbol.h"

struct sFreeArea {
	int32_t nOrg;
	int32_t nSize;
	struct sFreeArea *pPrev, *pNext;
};

struct sSectionAttributes {
	const char *name;
	/* bank + offset = bank originally stored in a section struct */
	int32_t bank;
	int32_t offset;
	int32_t minBank;
	int32_t bankCount;
};

struct sFreeArea *BankFree[BANK_INDEX_MAX];
int32_t MaxAvail[BANK_INDEX_MAX];
int32_t MaxBankUsed;
int32_t MaxWBankUsed;
int32_t MaxSBankUsed;
int32_t MaxVBankUsed;

const enum eSectionType SECT_MIN = SECT_WRAM0;
const enum eSectionType SECT_MAX = SECT_OAM;
const struct sSectionAttributes SECT_ATTRIBUTES[] = {
	{"WRAM0", BANK_INDEX_WRAM0, 0, 0, BANK_COUNT_WRAM0},
	{"VRAM",  BANK_INDEX_VRAM,  0, 0, BANK_COUNT_VRAM},
	{"ROMX",  BANK_INDEX_ROMX, -BANK_MIN_ROMX, BANK_MIN_ROMX, BANK_COUNT_ROMX},
	{"ROM0",  BANK_INDEX_ROM0,  0, 0, BANK_COUNT_ROM0},
	{"HRAM",  BANK_INDEX_HRAM,  0, 0, BANK_COUNT_HRAM},
	{"WRAMX", BANK_INDEX_WRAMX, -BANK_MIN_WRAMX, BANK_MIN_WRAMX, BANK_COUNT_WRAMX},
	{"SRAM",  BANK_INDEX_SRAM,  0, 0, BANK_COUNT_SRAM},
	{"OAM",   BANK_INDEX_OAM,   0, 0, BANK_COUNT_OAM}
};

static void do_max_bank(enum eSectionType Type, int32_t nBank)
{
	switch (Type) {
	case SECT_ROMX:
		if (nBank > MaxBankUsed)
			MaxBankUsed = nBank;
		break;
	case SECT_WRAMX:
		if (nBank > MaxWBankUsed)
			MaxWBankUsed = nBank;
		break;
	case SECT_SRAM:
		if (nBank > MaxSBankUsed)
			MaxSBankUsed = nBank;
		break;
	case SECT_VRAM:
		if (nBank > MaxVBankUsed)
			MaxVBankUsed = nBank;
		break;
	default:
		break;
	}
}

void ensureSectionTypeIsValid(enum eSectionType type)
{
	if (type < SECT_MIN || type > SECT_MAX)
		errx(1, "%s: Invalid section type found: %d", __func__, type);
}

int BankIndexIsROM0(int32_t bank)
{
	return (bank >= BANK_INDEX_ROM0) &&
	       (bank < (BANK_INDEX_ROM0 + BANK_COUNT_ROM0));
}

int BankIndexIsROMX(int32_t bank)
{
	return (bank >= BANK_INDEX_ROMX) &&
	       (bank < (BANK_INDEX_ROMX + BANK_COUNT_ROMX));
}

int BankIndexIsWRAM0(int32_t bank)
{
	return (bank >= BANK_INDEX_WRAM0) &&
	       (bank < (BANK_INDEX_WRAM0 + BANK_COUNT_WRAM0));
}

int BankIndexIsWRAMX(int32_t bank)
{
	return (bank >= BANK_INDEX_WRAMX) &&
	       (bank < (BANK_INDEX_WRAMX + BANK_COUNT_WRAMX));
}

int BankIndexIsVRAM(int32_t bank)
{
	return (bank >= BANK_INDEX_VRAM) &&
	       (bank < (BANK_INDEX_VRAM + BANK_COUNT_VRAM));
}

int BankIndexIsOAM(int32_t bank)
{
	return (bank >= BANK_INDEX_OAM) &&
	       (bank < (BANK_INDEX_OAM + BANK_COUNT_OAM));
}

int BankIndexIsHRAM(int32_t bank)
{
	return (bank >= BANK_INDEX_HRAM) &&
	       (bank < (BANK_INDEX_HRAM + BANK_COUNT_HRAM));
}

int BankIndexIsSRAM(int32_t bank)
{
	return (bank >= BANK_INDEX_SRAM) &&
	       (bank < (BANK_INDEX_SRAM + BANK_COUNT_SRAM));
}

int32_t area_Avail(int32_t bank)
{
	int32_t r;
	struct sFreeArea *pArea;

	r = 0;
	pArea = BankFree[bank];

	while (pArea) {
		r += pArea->nSize;
		pArea = pArea->pNext;
	}

	return r;
}

int32_t area_doAlloc(struct sFreeArea *pArea, int32_t org, int32_t size)
{
	if ((org >= pArea->nOrg)
	    && ((org + size) <= (pArea->nOrg + pArea->nSize))) {

		if (org == pArea->nOrg) {
			pArea->nOrg += size;
			pArea->nSize -= size;
			return org;
		}

		if ((org + size) == (pArea->nOrg + pArea->nSize)) {
			pArea->nSize -= size;
			return org;
		}

		struct sFreeArea *pNewArea;

		pNewArea = malloc(sizeof(struct sFreeArea));

		if (pNewArea == NULL)
			err(1, "%s: Failed to allocate memory", __func__);

		*pNewArea = *pArea;
		pNewArea->pPrev = pArea;
		pArea->pNext = pNewArea;
		pArea->nSize = org - pArea->nOrg;
		pNewArea->nOrg = org + size;
		pNewArea->nSize -= size + pArea->nSize;

		return org;
	}

	return -1;
}

int32_t area_AllocAbs(struct sFreeArea **ppArea, int32_t org, int32_t size)
{
	struct sFreeArea *pArea;

	pArea = *ppArea;
	while (pArea) {
		int32_t result = area_doAlloc(pArea, org, size);

		if (result != -1)
			return result;

		ppArea = &(pArea->pNext);
		pArea = *ppArea;
	}

	return -1;
}

int32_t area_AllocAbsAnyBank(int32_t org, int32_t size, enum eSectionType type)
{
	ensureSectionTypeIsValid(type);

	int32_t startBank = SECT_ATTRIBUTES[type].bank;
	int32_t bankCount = SECT_ATTRIBUTES[type].bankCount;

	for (int32_t i = 0; i < bankCount; i++) {
		if (area_AllocAbs(&BankFree[startBank + i], org, size) != -1)
			return startBank + i;
	}

	return -1;
}

int32_t area_Alloc(struct sFreeArea **ppArea, int32_t size, int32_t alignment)
{
	struct sFreeArea *pArea;

	if (alignment < 1)
		alignment = 1;

	pArea = *ppArea;
	while (pArea) {
		int32_t org = pArea->nOrg;

		if (org % alignment)
			org += alignment;

		org -= org % alignment;

		int32_t result = area_doAlloc(pArea, org, size);

		if (result != -1)
			return result;

		ppArea = &(pArea->pNext);
		pArea = *ppArea;
	}

	return -1;
}

int32_t area_AllocAnyBank(int32_t size, int32_t alignment,
			  enum eSectionType type)
{
	ensureSectionTypeIsValid(type);

	int32_t i, org;
	int32_t startBank = SECT_ATTRIBUTES[type].bank;
	int32_t bankCount = SECT_ATTRIBUTES[type].bankCount;

	for (i = 0; i < bankCount; i++) {
		org = area_Alloc(&BankFree[startBank + i], size, alignment);
		if (org != -1)
			return ((startBank + i) << 16) | org;
	}

	return -1;
}

struct sSection *FindLargestSection(enum eSectionType type, bool bankFixed)
{
	struct sSection *pSection, *r = NULL;
	int32_t nLargest = 0;
	int32_t nLargestAlignment = 0;

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0 && pSection->Type == type
		    && (bankFixed ^ (pSection->nBank == -1))) {
			if (pSection->nAlign > nLargestAlignment
			    || (pSection->nAlign == nLargestAlignment
				&& pSection->nByteSize > nLargest)) {

				nLargest = pSection->nByteSize;
				nLargestAlignment = pSection->nAlign;
				r = pSection;
			}
		}
		pSection = pSection->pNext;
	}

	return r;
}

int32_t IsSectionNameInUse(const char *name)
{
	const struct sSection *pSection = pSections;

	while (pSection) {
		if (strcmp(pSection->pzName, name) == 0)
			return 1;

		pSection = pSection->pNext;
	}

	return 0;
}

struct sSection *GetSectionByName(const char *name)
{
	struct sSection *pSection = pSections;

	while (pSection) {
		if (strcmp(pSection->pzName, name) == 0)
			return pSection;

		pSection = pSection->pNext;
	}

	return NULL;
}

int32_t IsSectionSameTypeBankAndFloating(const char *name,
					 enum eSectionType type, int32_t bank)
{
	const struct sSection *pSection;

	for (pSection = pSections; pSection; pSection = pSection->pNext) {
		/* Skip if it has already been assigned */
		if (pSection->oAssigned == 1)
			continue;

		/* Check if it has the same name */
		if (strcmp(pSection->pzName, name) != 0)
			continue;

		/*
		 * The section has the same name, now check if there is a
		 * mismatch or not.
		 */

		/* Section must be floating in source */
		if (pSection->nOrg != -1 || pSection->nAlign != 1)
			return 0;

		/* It must have the same type in source and linkerscript */
		if (pSection->Type != type)
			return 0;

		/* Bank number must be unassigned in source or equal */
		if (pSection->nBank != -1 && pSection->nBank != bank)
			return 0;

		return 1;
	}

	errx(1, "Section \"%s\" not found (or already used).\n", name);
}

uint32_t AssignSectionAddressAndBankByName(const char *name, uint32_t address,
					   int32_t bank)
{
	struct sSection *pSection;

	for (pSection = pSections; pSection; pSection = pSection->pNext) {
		/* Skip if it has already been assigned */
		if (pSection->oAssigned == 1)
			continue;

		/* Check if it has the same name */
		if (strcmp(pSection->pzName, name) != 0)
			continue;

		/* Section has been found. */

		/*
		 * A section can be left as floating in the code if the location
		 * is assigned in the linkerscript.
		 */
		if (pSection->nOrg != -1 || pSection->nAlign != 1) {
			errx(1, "Section \"%s\" from linkerscript isn't floating.\n",
			     name);
		}

		/* The bank can be left as unassigned or be the same */
		if (pSection->nBank != -1 && pSection->nBank != bank) {
			errx(1, "Section \"%s\" from linkerscript has different bank number than in the source.\n",
			     name);
		}

		pSection->nOrg = address;
		pSection->nBank = bank;
		pSection->nAlign = -1;

		return pSection->nByteSize;
	}

	errx(1, "Section \"%s\" not found (or already used).\n", name);
}

bool VerifyAndSetBank(struct sSection *pSection)
{
	enum eSectionType Type = pSection->Type;

	ensureSectionTypeIsValid(Type);

	if (pSection->nBank >= SECT_ATTRIBUTES[Type].minBank) {
		if (pSection->nBank < SECT_ATTRIBUTES[Type].minBank
				    + SECT_ATTRIBUTES[Type].bankCount) {
			pSection->nBank += SECT_ATTRIBUTES[Type].bank
					 + SECT_ATTRIBUTES[Type].offset;
			return true;
		}
	}

	return false;
}

void AssignFixedBankSections(enum eSectionType type)
{
	ensureSectionTypeIsValid(type);

	struct sSection *pSection;

	while ((pSection = FindLargestSection(type, true))) {
		if (VerifyAndSetBank(pSection)) {
			pSection->nOrg = area_Alloc(&BankFree[pSection->nBank],
						    pSection->nByteSize,
						    pSection->nAlign);
			if (pSection->nOrg != -1) {
				pSection->oAssigned = 1;
				do_max_bank(pSection->Type, pSection->nBank);
				continue;
			}
		}

		if (pSection->nAlign <= 1) {
			errx(1, "Unable to place '%s' (%s section) in bank $%02lX",
			     pSection->pzName,
			     SECT_ATTRIBUTES[pSection->Type].name,
			     pSection->nBank);
		} else {
			errx(1, "Unable to place '%s' (%s section) in bank $%02lX (with $%lX-byte alignment)",
			     pSection->pzName,
			     SECT_ATTRIBUTES[pSection->Type].name,
			     pSection->nBank, pSection->nAlign);
		}
	}
}

void AssignFloatingBankSections(enum eSectionType type)
{
	ensureSectionTypeIsValid(type);

	struct sSection *pSection;

	while ((pSection = FindLargestSection(type, false))) {
		int32_t org;

		org = area_AllocAnyBank(pSection->nByteSize, pSection->nAlign,
					type);

		if (org != -1) {
			if (options & OPT_OVERLAY)
				errx(1, "All sections must be fixed when using an overlay file.");

			pSection->nOrg = org & 0xFFFF;
			pSection->nBank = org >> 16;
			pSection->oAssigned = 1;
			do_max_bank(pSection->Type, pSection->nBank);
		} else {
			const char *locality = "anywhere";

			if (SECT_ATTRIBUTES[pSection->Type].bankCount > 1)
				locality = "in any bank";

			if (pSection->nAlign <= 1) {
				errx(1, "Unable to place '%s' (%s section) %s",
				     pSection->pzName,
				     SECT_ATTRIBUTES[type].name, locality);
			} else {
				errx(1, "Unable to place '%s' (%s section) %s (with $%lX-byte alignment)",
				     pSection->pzName,
				     SECT_ATTRIBUTES[type].name, locality,
				     pSection->nAlign);
			}
		}
	}
}

char *tzLinkerscriptName;

void SetLinkerscriptName(char *tzLinkerscriptFile)
{
	tzLinkerscriptName = tzLinkerscriptFile;
}

void AssignSections(void)
{
	int32_t i;
	struct sSection *pSection;

	MaxBankUsed = 0;

	/*
	 * Initialize the memory areas
	 */

	for (i = 0; i < BANK_INDEX_MAX; i += 1) {
		BankFree[i] = malloc(sizeof(*BankFree[i]));

		if (!BankFree[i]) {
			errx(1, "%s: Couldn't allocate mem for bank %d",
			     __func__, i);
		}

		if (BankIndexIsROM0(i)) {
			/* ROM0 bank */
			BankFree[i]->nOrg = 0x0000;
			if (options & OPT_TINY)
				BankFree[i]->nSize = 0x8000;
			else
				BankFree[i]->nSize = 0x4000;
		} else if (BankIndexIsROMX(i)) {
			/* Swappable ROM bank */
			BankFree[i]->nOrg = 0x4000;
			BankFree[i]->nSize = 0x4000;
		} else if (BankIndexIsWRAM0(i)) {
			/* WRAM */
			BankFree[i]->nOrg = 0xC000;
			if (options & OPT_CONTWRAM)
				BankFree[i]->nSize = 0x2000;
			else
				BankFree[i]->nSize = 0x1000;
		} else if (BankIndexIsSRAM(i)) {
			/* Swappable SRAM bank */
			BankFree[i]->nOrg = 0xA000;
			BankFree[i]->nSize = 0x2000;
		} else if (BankIndexIsWRAMX(i)) {
			/* Swappable WRAM bank */
			BankFree[i]->nOrg = 0xD000;
			BankFree[i]->nSize = 0x1000;
		} else if (BankIndexIsVRAM(i)) {
			/* Swappable VRAM bank */
			BankFree[i]->nOrg = 0x8000;
			if (options & OPT_DMG_MODE && i != BANK_INDEX_VRAM)
				BankFree[i]->nSize = 0;
			else
				BankFree[i]->nSize = 0x2000;
		} else if (BankIndexIsOAM(i)) {
			BankFree[i]->nOrg = 0xFE00;
			BankFree[i]->nSize = 0x00A0;
		} else if (BankIndexIsHRAM(i)) {
			/* HRAM */
			BankFree[i]->nOrg = 0xFF80;
			BankFree[i]->nSize = 0x007F;
		} else {
			errx(1, "%s: Unknown bank type %d", __func__, i);
		}

		MaxAvail[i] = BankFree[i]->nSize;
		BankFree[i]->pPrev = NULL;
		BankFree[i]->pNext = NULL;
	}

	/*
	 * First, let's parse the linkerscript.
	 */

	if (tzLinkerscriptName) {
		script_InitSections();
		script_Parse(tzLinkerscriptName);
	}

	/*
	 * Second, let's assign all the fixed sections...
	 */

	for (pSection = pSections ; pSection; pSection = pSection->pNext) {
		if (!((pSection->nOrg != -1 || pSection->nBank != -1)
		    && pSection->oAssigned == 0))
			continue;

		/* User wants to have a say... */

		switch (pSection->Type) {
		case SECT_WRAM0:
		case SECT_HRAM:
		case SECT_ROM0:
		case SECT_OAM:
			pSection->nBank = SECT_ATTRIBUTES[pSection->Type].bank;
			if (area_AllocAbs(&BankFree[pSection->nBank],
					  pSection->nOrg,
					  pSection->nByteSize) == -1) {
				errx(1, "Unable to place '%s' (%s section) at $%X",
				     pSection->pzName,
				     SECT_ATTRIBUTES[pSection->Type].name,
				     pSection->nOrg);
			}
			pSection->oAssigned = 1;
			break;

		case SECT_SRAM:
		case SECT_WRAMX:
		case SECT_VRAM:
		case SECT_ROMX:
			if (!(pSection->nBank != -1 && pSection->nOrg != -1))
				break;

			if (VerifyAndSetBank(pSection) &&
			    area_AllocAbs(&BankFree[pSection->nBank],
					  pSection->nOrg,
					  pSection->nByteSize) != -1) {
				do_max_bank(pSection->Type, pSection->nBank);
				pSection->oAssigned = 1;
			} else {
				errx(1, "Unable to place '%s' (%s section) at $%X in bank $%02X",
				     pSection->pzName,
				     SECT_ATTRIBUTES[pSection->Type].name,
				     pSection->nOrg, pSection->nBank);
			}
			break;
		}
	}

	/*
	 * Next, let's assign all the bankfixed ONLY sections...
	 */
	for (enum eSectionType i = SECT_MIN; i <= SECT_MAX; i++)
		AssignFixedBankSections(i);

	/*
	 * Now, let's assign all the floating bank but fixed ROMX sections...
	 */

	for (pSection = pSections ; pSection; pSection = pSection->pNext) {
		if (!(pSection->oAssigned == 0
		    && pSection->nOrg != -1 && pSection->nBank == -1))
			continue;

		if (options & OPT_OVERLAY) {
			errx(1, "All sections must be fixed when using an overlay file: '%s'",
			     pSection->pzName);
		}

		switch (pSection->Type) {
		case SECT_ROMX:
		case SECT_VRAM:
		case SECT_SRAM:
		case SECT_WRAMX:
			pSection->nBank =
				area_AllocAbsAnyBank(pSection->nOrg,
						     pSection->nByteSize,
						     pSection->Type);

			if (pSection->nBank == -1) {
				errx(1, "Unable to place '%s' (%s section) at $%X in any bank",
				     pSection->pzName,
				     SECT_ATTRIBUTES[pSection->Type].name,
				     pSection->nOrg);
			}
			pSection->oAssigned = 1;
			do_max_bank(pSection->Type, pSection->nBank);
			break;

		default: /* Handle other sections later */
			break;
		}
	}

	/*
	 * OK, all that nasty stuff is done so let's assign all the other
	 * sections
	 */
	for (enum eSectionType i = SECT_MIN; i <= SECT_MAX; i++)
		AssignFloatingBankSections(i);
}

void CreateSymbolTable(void)
{
	const struct sSection *pSect;

	sym_Init();

	pSect = pSections;

	while (pSect) {
		int32_t i;

		i = pSect->nNumberOfSymbols;

		while (i--) {
			const struct sSymbol *tSymbol = pSect->tSymbols[i];

			if ((tSymbol->Type == SYM_EXPORT) &&
			    ((tSymbol->pSection == pSect) ||
				(tSymbol->pSection == NULL))) {
				if (tSymbol->pSection == NULL)
					sym_CreateSymbol(
						tSymbol->pzName,
						tSymbol->nOffset,
						-1,
						tSymbol->pzObjFileName,
						tSymbol->pzFileName,
						tSymbol->nFileLine);
				else
					sym_CreateSymbol(
						tSymbol->pzName,
						pSect->nOrg + tSymbol->nOffset,
						pSect->nBank,
						tSymbol->pzObjFileName,
						tSymbol->pzFileName,
						tSymbol->nFileLine);
			}
		}
		pSect = pSect->pNext;
	}
}
