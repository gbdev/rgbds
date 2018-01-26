/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINK_ASSIGN_H
#define RGBDS_LINK_ASSIGN_H

#include <stdint.h>

#include "common.h"
#include "mylink.h"

/* Bank numbers as seen by the linker */
enum eBankDefine {
	BANK_INDEX_ROM0  = 0,
	BANK_INDEX_ROMX  = BANK_INDEX_ROM0  + BANK_COUNT_ROM0,
	BANK_INDEX_WRAM0 = BANK_INDEX_ROMX  + BANK_COUNT_ROMX,
	BANK_INDEX_WRAMX = BANK_INDEX_WRAM0 + BANK_COUNT_WRAM0,
	BANK_INDEX_VRAM  = BANK_INDEX_WRAMX + BANK_COUNT_WRAMX,
	BANK_INDEX_OAM   = BANK_INDEX_VRAM  + BANK_COUNT_VRAM,
	BANK_INDEX_HRAM  = BANK_INDEX_OAM   + BANK_COUNT_OAM,
	BANK_INDEX_SRAM  = BANK_INDEX_HRAM  + BANK_COUNT_HRAM,
	BANK_INDEX_MAX   = BANK_INDEX_SRAM  + BANK_COUNT_SRAM
};

extern int32_t MaxBankUsed;
extern int32_t MaxAvail[BANK_INDEX_MAX];

int32_t area_Avail(int32_t bank);
void AssignSections(void);
void CreateSymbolTable(void);
struct sSection *GetSectionByName(const char *name);
int32_t IsSectionNameInUse(const char *name);
void SetLinkerscriptName(char *tzLinkerscriptFile);
int32_t IsSectionSameTypeBankAndFloating(const char *name,
					 enum eSectionType type, int32_t bank);
uint32_t AssignSectionAddressAndBankByName(const char *name, uint32_t address,
					   int32_t bank);

int BankIndexIsROM0(int32_t bank);
int BankIndexIsROMX(int32_t bank);
int BankIndexIsWRAM0(int32_t bank);
int BankIndexIsWRAMX(int32_t bank);
int BankIndexIsVRAM(int32_t bank);
int BankIndexIsOAM(int32_t bank);
int BankIndexIsHRAM(int32_t bank);
int BankIndexIsSRAM(int32_t bank);

#endif /* RGBDS_LINK_ASSIGN_H */
