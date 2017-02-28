#ifndef RGBDS_LINK_ASSIGN_H
#define RGBDS_LINK_ASSIGN_H

#include "types.h"

enum eBankDefine {
	BANK_ROM0 = 0,
	BANK_ROMX,
	BANK_WRAM0 = 512,
	BANK_WRAMX,
	BANK_VRAM = 520,
	BANK_HRAM = 522,
	BANK_SRAM = 523
};

enum eBankCount {
	BANK_COUNT_ROM0 = 1,
	BANK_COUNT_ROMX = 511,
	BANK_COUNT_WRAM0 = 1,
	BANK_COUNT_WRAMX = 7,
	BANK_COUNT_VRAM = 2,
	BANK_COUNT_HRAM = 1,
	BANK_COUNT_SRAM = 16
};

#define MAXBANKS	(BANK_COUNT_ROM0 + BANK_COUNT_ROMX + BANK_COUNT_WRAM0 + BANK_COUNT_WRAMX \
					+ BANK_COUNT_VRAM + BANK_COUNT_HRAM + BANK_COUNT_SRAM)

extern SLONG area_Avail(SLONG bank);
extern void AssignSections(void);
extern void CreateSymbolTable(void);
extern SLONG MaxBankUsed;
extern SLONG MaxAvail[MAXBANKS];

#endif
