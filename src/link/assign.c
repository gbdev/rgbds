#include <stdio.h>
#include <stdlib.h>

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

struct sFreeArea *BankFree[MAXBANKS];
SLONG MaxAvail[MAXBANKS];
SLONG MaxBankUsed;
SLONG MaxWBankUsed;
SLONG MaxSBankUsed;
SLONG MaxVBankUsed;

#define DOMAXBANK(x)	{if( (x)>MaxBankUsed ) MaxBankUsed=(x);}
#define DOMAXWBANK(x)	{if( (x)>MaxWBankUsed ) MaxWBankUsed=(x);}
#define DOMAXSBANK(x)	{if( (x)>MaxSBankUsed ) MaxSBankUsed=(x);}
#define DOMAXVBANK(x)	{if( (x)>MaxVBankUsed ) MaxVBankUsed=(x);}

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
				return (org);
			} else {
				if ((org + size - 1) ==
				    (pArea->nOrg + pArea->nSize - 1)) {
					pArea->nSize -= size;
					return (org);
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

						return (org);
					} else {
						err(1, NULL);
					}
				}
			}
		}
		ppArea = &(pArea->pNext);
		pArea = *ppArea;
	}

	return (-1);
}

SLONG
area_AllocAbsSRAMAnyBank(SLONG org, SLONG size)
{
	int i;
	for (i = 0; i < 4; ++i) {
		if (area_AllocAbs(&BankFree[BANK_SRAM + i], org, size) == org) {
			return BANK_SRAM + i;
		}
	}

	return -1;
}

SLONG
area_AllocAbsWRAMAnyBank(SLONG org, SLONG size)
{
	SLONG i;

	for (i = 1; i <= 7; i += 1) {
		if (area_AllocAbs(&BankFree[BANK_WRAMX + i - 1], org, size) == org) {
			return BANK_WRAMX + i - 1;
		}
	}

	return -1;
}

SLONG
area_AllocAbsVRAMAnyBank(SLONG org, SLONG size)
{
	if (area_AllocAbs(&BankFree[BANK_VRAM], org, size) == org) {
		return BANK_VRAM;
	}
	if (area_AllocAbs(&BankFree[BANK_VRAM + 1], org, size) == org) {
		return BANK_VRAM + 1;
	}

	return -1;
}

SLONG 
area_AllocAbsROMXAnyBank(SLONG org, SLONG size)
{
	SLONG i;

	for (i = 1; i <= 511; i += 1) {
		if (area_AllocAbs(&BankFree[i], org, size) == org)
			return (i);
	}

	return (-1);
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
area_AllocVRAMAnyBank(SLONG size)
{
	SLONG i, org;

	for (i = BANK_VRAM; i <= BANK_VRAM + 1; i += 1) {
		if ((org = area_Alloc(&BankFree[i], size)) != -1)
			return ((i << 16) | org);
	}

	return (-1);
}

SLONG
area_AllocSRAMAnyBank(SLONG size)
{
	SLONG i, org;
	for (i = 0; i < 4; ++i) {
		if ((org = area_Alloc(&BankFree[BANK_SRAM + i], size)) != -1) {
			return (i << 16) | org;
		}
	}

	return -1;
}

SLONG
area_AllocWRAMAnyBank(SLONG size)
{
	SLONG i, org;

	for (i = 1; i <= 7; i += 1) {
		if ((org = area_Alloc(&BankFree[BANK_WRAMX + i - 1], size)) != -1) {
			return (i << 16) | org;
		}
	}

	return -1;
}

SLONG 
area_AllocROMXAnyBank(SLONG size)
{
	SLONG i, org;

	for (i = 1; i <= 511; i += 1) {
		if ((org = area_Alloc(&BankFree[i], size)) != -1)
			return ((i << 16) | org);
	}

	return (-1);
}

struct sSection *
FindLargestWRAM(void)
{
	struct sSection *pSection, *r = NULL;
	SLONG nLargest = 0;

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0 && pSection->Type == SECT_WRAMX) {
			if (pSection->nByteSize > nLargest) {
				nLargest = pSection->nByteSize;
				r = pSection;
			}
		}
		pSection = pSection->pNext;
	}
	return r;
}

struct sSection *
FindLargestVRAM(void)
{
	struct sSection *pSection, *r = NULL;
	SLONG nLargest = 0;

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0 && pSection->Type == SECT_VRAM) {
			if (pSection->nByteSize > nLargest) {
				nLargest = pSection->nByteSize;
				r = pSection;
			}
		}
		pSection = pSection->pNext;
	}
	return r;
}

struct sSection *
FindLargestSRAM(void)
{
	struct sSection *pSection, *r = NULL;
	SLONG nLargest = 0;

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0 && pSection->Type == SECT_SRAM) {
			if (pSection->nByteSize > nLargest) {
				nLargest = pSection->nByteSize;
				r = pSection;
			}
		}
		pSection = pSection->pNext;
	}
	return r;
}

struct sSection *
FindLargestCode(void)
{
	struct sSection *pSection, *r = NULL;
	SLONG nLargest = 0;

	pSection = pSections;
	while (pSection) {
		if (pSection->oAssigned == 0 && pSection->Type == SECT_ROMX) {
			if (pSection->nByteSize > nLargest) {
				nLargest = pSection->nByteSize;
				r = pSection;
			}
		}
		pSection = pSection->pNext;
	}
	return (r);
}

void
AssignVRAMSections(void)
{
	struct sSection *pSection;

	while ((pSection = FindLargestVRAM())) {
		SLONG org;

		if ((org = area_AllocVRAMAnyBank(pSection->nByteSize)) != -1) {
			pSection->nOrg = org & 0xFFFF;
			pSection->nBank = org >> 16;
			pSection->oAssigned = 1;
			DOMAXVBANK(pSection->nBank);
		} else {
			errx(1, "Unable to place VRAM section anywhere");
		}
	}
}

void
AssignSRAMSections(void)
{
	struct sSection *pSection;

	while ((pSection = FindLargestSRAM())) {
		SLONG org;

		if ((org = area_AllocSRAMAnyBank(pSection->nByteSize)) != -1) {
			pSection->nOrg = org & 0xFFFF;
			pSection->nBank = org >> 16;
			pSection->nBank += BANK_SRAM;
			pSection->oAssigned = 1;
			DOMAXSBANK(pSection->nBank);
		} else {
			errx(1, "Unable to place SRAM section anywhere");
		}
	}
}

void
AssignWRAMSections(void)
{
	struct sSection *pSection;

	while ((pSection = FindLargestWRAM())) {
		SLONG org;

		if ((org = area_AllocWRAMAnyBank(pSection->nByteSize)) != -1) {
			pSection->nOrg = org & 0xFFFF;
			pSection->nBank = org >> 16;
			pSection->nBank += BANK_WRAMX - 1;
			pSection->oAssigned = 1;
			DOMAXWBANK(pSection->nBank);
		} else {
			errx(1, "Unable to place WRAMX section anywhere");
		}
	}
}

void 
AssignCodeSections(void)
{
	struct sSection *pSection;

	while ((pSection = FindLargestCode())) {
		SLONG org;

		if ((org = area_AllocROMXAnyBank(pSection->nByteSize)) != -1) {
			pSection->nOrg = org & 0xFFFF;
			pSection->nBank = org >> 16;
			pSection->oAssigned = 1;
			DOMAXBANK(pSection->nBank);
		} else {
			errx(1, "Unable to place ROMX section anywhere");
		}
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

		if (i == 0) {
			/* ROM0 bank */
			BankFree[i]->nOrg = 0x0000;
			if (options & OPT_SMALL) {
				BankFree[i]->nSize = 0x8000;
				MaxAvail[i] = 0x8000;
			} else {
				BankFree[i]->nSize = 0x4000;
				MaxAvail[i] = 0x4000;
			}
		} else if (i >= 1 && i <= 511) {
			/* Swappable ROM bank */
			BankFree[i]->nOrg = 0x4000;
			/*
			 * Now, this shouldn't really be necessary... but for
			 * good measure we'll do it anyway.
			 */
			if (options & OPT_SMALL) {
				BankFree[i]->nSize = 0;
				MaxAvail[i] = 0;
			} else {
				BankFree[i]->nSize = 0x4000;
				MaxAvail[i] = 0x4000;
			}
		} else if (i == BANK_WRAM0) {
			/* WRAM */
			BankFree[i]->nOrg = 0xC000;
			BankFree[i]->nSize = 0x1000;
			MaxAvail[i] = 0x1000;
		} else if (i >= BANK_SRAM && i <= BANK_SRAM + 3) {
			/* Swappable SRAM bank */
			BankFree[i]->nOrg = 0xA000;
			BankFree[i]->nSize = 0x2000;
			MaxAvail[i] = 0x2000;
		} else if (i >= BANK_WRAMX && i <= BANK_WRAMX + 6) {
			/* Swappable WRAM bank */
			BankFree[i]->nOrg = 0xD000;
			BankFree[i]->nSize = 0x1000;
			MaxAvail[i] = 0x1000;
		} else if (i == BANK_VRAM || i == BANK_VRAM + 1) {
			/* Swappable VRAM bank */
			BankFree[i]->nOrg = 0x8000;
			BankFree[i]->nSize = 0x2000;
			MaxAvail[i] = 0x2000;
		} else if (i == BANK_HRAM) {
			/* HRAM */
			BankFree[i]->nOrg = 0xFF80;
			BankFree[i]->nSize = 0x007F;
			MaxAvail[i] = 0x007F;
		}
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

			switch (pSection->Type) {
			case SECT_WRAM0:
				if (area_AllocAbs
				    (&BankFree[BANK_WRAM0], pSection->nOrg,
					pSection->nByteSize) != pSection->nOrg) {
					errx(1,
					    "Unable to load fixed WRAM0 "
					    "section at $%lX", pSection->nOrg);
				}
				pSection->oAssigned = 1;
				pSection->nBank = BANK_WRAM0;
				break;
			case SECT_HRAM:
				if (area_AllocAbs
				    (&BankFree[BANK_HRAM], pSection->nOrg,
					pSection->nByteSize) != pSection->nOrg) {
					errx(1, "Unable to load fixed HRAM "
					    "section at $%lX", pSection->nOrg);
				}
				pSection->oAssigned = 1;
				pSection->nBank = BANK_HRAM;
				break;
			case SECT_SRAM:
				if (pSection->nBank == -1) {
					/*
					 * User doesn't care which bank.
					 * Therefore he must here be specifying
					 * position within the bank.
					 * Defer until later.
					 */
					;
				} else {
					/*
					 * User specified which bank to use.
					 * Does he also care about position
					 * within the bank?
					 */
					if (pSection->nOrg == -1) {
						/*
						 * Nope, any position will do
						 * Again, we'll do that later
						 *
						 */
						;
					} else {
						/*
						 * Bank and position within the
						 * bank are hardcoded.
						 */

						if (pSection->nBank >= 0
						    && pSection->nBank <= 3) {
							pSection->nBank +=
							    BANK_SRAM;
							if (area_AllocAbs
							    (&BankFree
							    [pSection->nBank],
							    pSection->nOrg,
							    pSection->nByteSize)
							    != pSection->nOrg) {
								errx(1,
"Unable to load fixed SRAM section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
							}
							DOMAXVBANK(pSection->
							    nBank);
							pSection->oAssigned = 1;
						} else {
							errx(1,
"Unable to load fixed SRAM section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
						}
					}
				}
				break;
			case SECT_WRAMX:
				if (pSection->nBank == -1) {
					/*
					 * User doesn't care which bank.
					 * Therefore he must here be specifying
					 * position within the bank.
					 * Defer until later.
					 */
					;
				} else {
					/*
					 * User specified which bank to use.
					 * Does he also care about position
					 * within the bank?
					 */
					if (pSection->nOrg == -1) {
						/*
						 * Nope, any position will do
						 * Again, we'll do that later
						 *
						 */
						;
					} else {
						/*
						 * Bank and position within the
						 * bank are hardcoded.
						 */

						if (pSection->nBank >= 0
						    && pSection->nBank <= 6) {
							pSection->nBank +=
							    BANK_WRAMX;
							if (area_AllocAbs
							    (&BankFree
							    [pSection->nBank],
							    pSection->nOrg,
							    pSection->nByteSize)
							    != pSection->nOrg) {
								errx(1,
"Unable to load fixed WRAMX section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
							}
							DOMAXWBANK(pSection->
							    nBank);
							pSection->oAssigned = 1;
						} else {
							errx(1,
"Unable to load fixed WRAMX section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
						}
					}
				}
				break;
			case SECT_VRAM:
				if (pSection->nBank == -1) {
					/*
					 * User doesn't care which bank.
					 * Therefore he must here be specifying
					 * position within the bank.
					 * Defer until later.
					 */
					;
				} else {
					/*
					 * User specified which bank to use.
					 * Does he also care about position
					 * within the bank?
					 */
					if (pSection->nOrg == -1) {
						/*
						 * Nope, any position will do
						 * Again, we'll do that later
						 *
						 */
						;
					} else {
						/*
						 * Bank and position within the
						 * bank are hardcoded.
						 */

						if (pSection->nBank >= 0
						    && pSection->nBank <= 1) {
							pSection->nBank +=
							    BANK_VRAM;
							if (area_AllocAbs
							    (&BankFree
							    [pSection->nBank],
							    pSection->nOrg,
							    pSection->nByteSize)
							    != pSection->nOrg) {
								errx(1,
"Unable to load fixed VRAM section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
							}
							DOMAXVBANK(pSection->
							    nBank);
							pSection->oAssigned = 1;
						} else {
							errx(1,
"Unable to load fixed VRAM section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
						}
					}
				}
				break;
			case SECT_ROM0:
				if (area_AllocAbs
				    (&BankFree[BANK_ROM0], pSection->nOrg,
					pSection->nByteSize) != pSection->nOrg) {
					errx(1, "Unable to load fixed ROM0 "
					    "section at $%lX", pSection->nOrg);
				}
				pSection->oAssigned = 1;
				pSection->nBank = BANK_ROM0;
				break;
			case SECT_ROMX:
				if (pSection->nBank == -1) {
					/*
					 * User doesn't care which bank, so he must want to
					 * decide which position within that bank.
					 * We'll do that at a later stage when the really
					 * hardcoded things are allocated
					 *
					 */
				} else {
					/*
					 * User wants to decide which bank we use
					 * Does he care about the position as well?
					 *
					 */

					if (pSection->nOrg == -1) {
						/*
						 * Nope, any position will do
						 * Again, we'll do that later
						 *
						 */
					} else {
						/*
						 * How hardcore can you possibly get? Why does
						 * he even USE this package? Yeah let's just
						 * direct address everything, shall we?
						 * Oh well, the customer is always right
						 *
						 */

						if (pSection->nBank >= 1
						    && pSection->nBank <= 511) {
							if (area_AllocAbs
							    (&BankFree
								[pSection->nBank],
								pSection->nOrg,
								pSection->
								nByteSize) !=
							    pSection->nOrg) {
								errx(1,
								    "Unable to load fixed ROMX section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
							}
							DOMAXBANK(pSection->
							    nBank);
							pSection->oAssigned = 1;
						} else {
							errx(1,
							"Unable to load fixed ROMX section at $%lX in bank $%02lX", pSection->nOrg, pSection->nBank);
						}
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
		    && pSection->Type == SECT_ROMX
		    && pSection->nOrg == -1 && pSection->nBank != -1) {
			/* User wants to have a say... and he's pissed */
			if (pSection->nBank >= 1 && pSection->nBank <= 511) {
				if ((pSection->nOrg =
					area_Alloc(&BankFree[pSection->nBank],
					    pSection->nByteSize)) == -1) {
					errx(1,
					"Unable to load fixed ROMX section into bank $%02lX", pSection->nBank);
				}
				pSection->oAssigned = 1;
				DOMAXBANK(pSection->nBank);
			} else {
				errx(1, "Unable to load fixed ROMX section into bank $%02lX", pSection->nBank);
			}
		} else if (pSection->oAssigned == 0
		    && pSection->Type == SECT_SRAM
		    && pSection->nOrg == -1 && pSection->nBank != -1) {
			pSection->nBank += BANK_SRAM;
			/* User wants to have a say... and he's pissed */
			if (pSection->nBank >= BANK_SRAM && pSection->nBank <= BANK_SRAM + 3) {
				if ((pSection->nOrg =
					area_Alloc(&BankFree[pSection->nBank],
					    pSection->nByteSize)) == -1) {
					errx(1, "Unable to load fixed SRAM section into bank $%02lX", pSection->nBank);
				}
				pSection->oAssigned = 1;
				DOMAXSBANK(pSection->nBank);
			} else {
				errx(1, "Unable to load fixed VRAM section into bank $%02lX", pSection->nBank);
			}
		} else if (pSection->oAssigned == 0
		    && pSection->Type == SECT_VRAM
		    && pSection->nOrg == -1 && pSection->nBank != -1) {
			pSection->nBank += BANK_VRAM;
			/* User wants to have a say... and he's pissed */
			if (pSection->nBank >= BANK_VRAM && pSection->nBank <= BANK_VRAM + 1) {
				if ((pSection->nOrg =
					area_Alloc(&BankFree[pSection->nBank],
					    pSection->nByteSize)) == -1) {
					errx(1, "Unable to load fixed VRAM section into bank $%02lX", pSection->nBank);
				}
				pSection->oAssigned = 1;
				DOMAXVBANK(pSection->nBank);
			} else {
				errx(1, "Unable to load fixed VRAM section into bank $%02lX", pSection->nBank);
			}
		} else if (pSection->oAssigned == 0
		    && pSection->Type == SECT_WRAMX
		    && pSection->nOrg == -1 && pSection->nBank != -1) {
			pSection->nBank += BANK_WRAMX;
			/* User wants to have a say... and he's pissed */
			if (pSection->nBank >= BANK_WRAMX && pSection->nBank <= BANK_WRAMX + 6) {
				if ((pSection->nOrg =
					area_Alloc(&BankFree[pSection->nBank],
					    pSection->nByteSize)) == -1) {
					errx(1, "Unable to load fixed WRAMX section into bank $%02lX", pSection->nBank - BANK_WRAMX);
				}
				pSection->oAssigned = 1;
				DOMAXWBANK(pSection->nBank);
			} else {
				errx(1, "Unable to load fixed WRAMX section into bank $%02lX", pSection->nBank - BANK_WRAMX);
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
		    && pSection->Type == SECT_ROMX
		    && pSection->nOrg != -1 && pSection->nBank == -1) {
			/* User wants to have a say... and he's back with a
			 * vengeance */
			if ((pSection->nBank =
				area_AllocAbsROMXAnyBank(pSection->nOrg,
				    pSection->nByteSize)) ==
			    -1) {
				errx(1, "Unable to load fixed ROMX section at $%lX into any bank", pSection->nOrg);
			}
			pSection->oAssigned = 1;
			DOMAXBANK(pSection->nBank);
		} else if (pSection->oAssigned == 0
		    && pSection->Type == SECT_VRAM
		    && pSection->nOrg != -1 && pSection->nBank == -1) {
			/* User wants to have a say... and he's back with a
			 * vengeance */
			if ((pSection->nBank =
				area_AllocAbsVRAMAnyBank(pSection->nOrg,
				    pSection->nByteSize)) ==
			    -1) {
				errx(1, "Unable to load fixed VRAM section at $%lX into any bank", pSection->nOrg);
			}
			pSection->oAssigned = 1;
			DOMAXVBANK(pSection->nBank);
		} else if (pSection->oAssigned == 0
		    && pSection->Type == SECT_SRAM
		    && pSection->nOrg != -1 && pSection->nBank == -1) {
			/* User wants to have a say... and he's back with a
			 * vengeance */
			if ((pSection->nBank =
				area_AllocAbsSRAMAnyBank(pSection->nOrg,
				    pSection->nByteSize)) ==
			    -1) {
				errx(1, "Unable to load fixed SRAM section at $%lX into any bank", pSection->nOrg);
			}
			pSection->oAssigned = 1;
			DOMAXSBANK(pSection->nBank);
		} else if (pSection->oAssigned == 0
		    && pSection->Type == SECT_WRAMX
		    && pSection->nOrg != -1 && pSection->nBank == -1) {
			/* User wants to have a say... and he's back with a
			 * vengeance */
			if ((pSection->nBank =
				area_AllocAbsWRAMAnyBank(pSection->nOrg,
				    pSection->nByteSize)) ==
			    -1) {
				errx(1, "Unable to load fixed WRAMX section at $%lX into any bank", pSection->nOrg);
			}
			pSection->oAssigned = 1;
			DOMAXWBANK(pSection->nBank);
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
				if ((pSection->nOrg =
					area_Alloc(&BankFree[BANK_WRAM0],
					    pSection->nByteSize)) == -1) {
					errx(1, "WRAM0 section too large");
				}
				pSection->nBank = BANK_WRAM0;
				pSection->oAssigned = 1;
				break;
			case SECT_HRAM:
				if ((pSection->nOrg =
					area_Alloc(&BankFree[BANK_HRAM],
					    pSection->nByteSize)) == -1) {
					errx(1, "HRAM section too large");
				}
				pSection->nBank = BANK_HRAM;
				pSection->oAssigned = 1;
				break;
			case SECT_SRAM:
				break;
			case SECT_VRAM:
				break;
			case SECT_WRAMX:
				break;
			case SECT_ROM0:
				if ((pSection->nOrg =
					area_Alloc(&BankFree[BANK_ROM0],
					    pSection->nByteSize)) == -1) {
					errx(1, "ROM0 section too large");
				}
				pSection->nBank = BANK_ROM0;
				pSection->oAssigned = 1;
				break;
			case SECT_ROMX:
				break;
			default:
				errx(1, "(INTERNAL) Unknown section type!");
				break;
			}
		}
		pSection = pSection->pNext;
	}

	AssignCodeSections();
	AssignVRAMSections();
	AssignWRAMSections();
	AssignSRAMSections();
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
