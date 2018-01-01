#ifndef RGBDS_LINK_ASSIGN_H
#define RGBDS_LINK_ASSIGN_H

#include <stdint.h>

#include "mylink.h"

enum eBankCount {
	BANK_COUNT_ROM0 = 1,
	BANK_COUNT_ROMX = 511,
	BANK_COUNT_WRAM0 = 1,
	BANK_COUNT_WRAMX = 7,
	BANK_COUNT_VRAM = 2,
	BANK_COUNT_OAM  = 1,
	BANK_COUNT_HRAM = 1,
	BANK_COUNT_SRAM = 16
};

enum eBankDefine {
	BANK_ROM0  = 0,
	BANK_ROMX  = BANK_ROM0  + BANK_COUNT_ROM0,
	BANK_WRAM0 = BANK_ROMX  + BANK_COUNT_ROMX,
	BANK_WRAMX = BANK_WRAM0 + BANK_COUNT_WRAM0,
	BANK_VRAM  = BANK_WRAMX + BANK_COUNT_WRAMX,
	BANK_OAM   = BANK_VRAM  + BANK_COUNT_VRAM,
	BANK_HRAM  = BANK_OAM   + BANK_COUNT_OAM,
	BANK_SRAM  = BANK_HRAM  + BANK_COUNT_HRAM
};

#define MAXBANKS	(BANK_COUNT_ROM0 + BANK_COUNT_ROMX	\
			+ BANK_COUNT_WRAM0 + BANK_COUNT_WRAMX	\
			+ BANK_COUNT_VRAM + BANK_COUNT_OAM	\
			+ BANK_COUNT_HRAM + BANK_COUNT_SRAM)

extern int32_t MaxBankUsed;
extern int32_t MaxAvail[MAXBANKS];

int32_t area_Avail(int32_t bank);
void AssignSections(void);
void CreateSymbolTable(void);
int32_t IsSectionNameInUse(const char *name);
void SetLinkerscriptName(char *tzLinkerscriptFile);
int32_t IsSectionSameTypeBankAndFloating(const char *name,
					 enum eSectionType type, int32_t bank);
uint32_t AssignSectionAddressAndBankByName(const char *name, uint32_t address,
					   int32_t bank);

#endif /* RGBDS_LINK_ASSIGN_H */
