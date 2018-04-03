/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2017-2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>
#include <string.h>

#include "extern/err.h"

#include "link/assign.h"
#include "link/mylink.h"

static struct {
	uint32_t address; /* current address to write sections to */
	uint32_t top_address; /* not inclusive */
	enum eSectionType type;
} bank[BANK_INDEX_MAX];

static int32_t current_bank = -1; /* Bank as seen by the bank array */
static int32_t current_real_bank = -1; /* bank as seen by the GB */

void script_InitSections(void)
{
	int32_t i;

	for (i = 0; i < BANK_INDEX_MAX; i++) {
		if (BankIndexIsROM0(i)) {
			/* ROM0 bank */
			bank[i].address = 0x0000;
			if (options & OPT_TINY)
				bank[i].top_address = 0x8000;
			else
				bank[i].top_address = 0x4000;
			bank[i].type = SECT_ROM0;
		} else if (BankIndexIsROMX(i)) {
			/* Swappable ROM bank */
			bank[i].address = 0x4000;
			bank[i].top_address = 0x8000;
			bank[i].type = SECT_ROMX;
		} else if (BankIndexIsWRAM0(i)) {
			/* WRAM */
			bank[i].address = 0xC000;
			if (options & OPT_CONTWRAM)
				bank[i].top_address = 0xE000;
			else
				bank[i].top_address = 0xD000;
			bank[i].type = SECT_WRAM0;
		} else if (BankIndexIsSRAM(i)) {
			/* Swappable SRAM bank */
			bank[i].address = 0xA000;
			bank[i].top_address = 0xC000;
			bank[i].type = SECT_SRAM;
		} else if (BankIndexIsWRAMX(i)) {
			/* Swappable WRAM bank */
			bank[i].address = 0xD000;
			bank[i].top_address = 0xE000;
			bank[i].type = SECT_WRAMX;
		} else if (BankIndexIsVRAM(i)) {
			/* Swappable VRAM bank */
			bank[i].address = 0x8000;
			bank[i].type = SECT_VRAM;
			if (options & OPT_DMG_MODE && i != BANK_INDEX_VRAM) {
				/* In DMG the only available bank is bank 0. */
				bank[i].top_address = 0x8000;
			} else {
				bank[i].top_address = 0xA000;
			}
		} else if (BankIndexIsOAM(i)) {
			/* OAM */
			bank[i].address = 0xFE00;
			bank[i].top_address = 0xFEA0;
			bank[i].type = SECT_OAM;
		} else if (BankIndexIsHRAM(i)) {
			/* HRAM */
			bank[i].address = 0xFF80;
			bank[i].top_address = 0xFFFF;
			bank[i].type = SECT_HRAM;
		} else {
			errx(1, "%s: Unknown bank type %d", __func__, i);
		}
	}
}

void script_SetCurrentSectionType(const char *type, uint32_t bank_num)
{
	if (strcmp(type, "ROM0") == 0) {
		if (bank_num != 0)
			errx(1, "Trying to assign a bank number to ROM0.\n");
		current_bank = BANK_INDEX_ROM0;
		current_real_bank = 0;
		return;
	} else if (strcmp(type, "ROMX") == 0) {
		if (bank_num == 0)
			errx(1, "ROMX index can't be 0.\n");
		if (bank_num > BANK_COUNT_ROMX) {
			errx(1, "ROMX index too big (%d > %d).\n", bank_num,
			     BANK_COUNT_ROMX);
		}
		current_bank = BANK_INDEX_ROMX + bank_num - 1;
		current_real_bank = bank_num;
		return;
	} else if (strcmp(type, "VRAM") == 0) {
		if (bank_num >= BANK_COUNT_VRAM) {
			errx(1, "VRAM index too big (%d >= %d).\n", bank_num,
			     BANK_COUNT_VRAM);
		}
		current_bank = BANK_INDEX_VRAM + bank_num;
		current_real_bank = bank_num;
		return;
	} else if (strcmp(type, "WRAM0") == 0) {
		if (bank_num != 0)
			errx(1, "Trying to assign a bank number to WRAM0.\n");

		current_bank = BANK_INDEX_WRAM0;
		current_real_bank = 0;
		return;
	} else if (strcmp(type, "WRAMX") == 0) {
		if (bank_num == 0)
			errx(1, "WRAMX index can't be 0.\n");
		if (bank_num > BANK_COUNT_WRAMX) {
			errx(1, "WRAMX index too big (%d > %d).\n", bank_num,
			     BANK_COUNT_WRAMX);
		}
		current_bank = BANK_INDEX_WRAMX + bank_num - 1;
		current_real_bank = bank_num;
		return;
	} else if (strcmp(type, "SRAM") == 0) {
		if (bank_num >= BANK_COUNT_SRAM) {
			errx(1, "SRAM index too big (%d >= %d).\n", bank_num,
			     BANK_COUNT_SRAM);
		}
		current_bank = BANK_INDEX_SRAM + bank_num;
		current_real_bank = bank_num;
		return;
	} else if (strcmp(type, "OAM") == 0) {
		if (bank_num != 0) {
			errx(1, "%s: Trying to assign a bank number to OAM.\n",
			     __func__);
		}
		current_bank = BANK_INDEX_OAM;
		current_real_bank = 0;
		return;
	} else if (strcmp(type, "HRAM") == 0) {
		if (bank_num != 0) {
			errx(1, "%s: Trying to assign a bank number to HRAM.\n",
			     __func__);
		}
		current_bank = BANK_INDEX_HRAM;
		current_real_bank = 0;
		return;
	}

	errx(1, "%s: Unknown section type \"%s\".\n", __func__, type);
}

void script_SetAddress(uint32_t addr)
{
	if (current_bank == -1)
		errx(1, "Trying to set an address without assigned bank\n");

	/* Make sure that we don't go back. */
	if (bank[current_bank].address > addr) {
		errx(1, "Trying to go to a previous address (0x%04X to 0x%04X)\n",
		     bank[current_bank].address, addr);
	}

	bank[current_bank].address = addr;

	/* Make sure we don't overflow */
	if (bank[current_bank].address >= bank[current_bank].top_address) {
		errx(1, "Bank overflowed (0x%04X >= 0x%04X)\n",
		     bank[current_bank].address,
		     bank[current_bank].top_address);
	}
}

void script_SetAlignment(uint32_t alignment)
{
	if (current_bank == -1)
		errx(1, "Trying to set an alignment without assigned bank\n");

	if (alignment > 15)
		errx(1, "Trying to set an alignment too big: %d\n", alignment);

	uint32_t size = 1 << alignment;
	uint32_t mask = size - 1;

	if (bank[current_bank].address & mask) {
		bank[current_bank].address &= ~mask;
		bank[current_bank].address += size;
	}

	/* Make sure we don't overflow */
	if (bank[current_bank].address >= bank[current_bank].top_address) {
		errx(1, "Bank overflowed (0x%04X >= 0x%04X)\n",
		     bank[current_bank].address,
		     bank[current_bank].top_address);
	}
}

void script_OutputSection(const char *section_name)
{
	if (current_bank == -1) {
		errx(1, "Trying to place section \"%s\" without assigned bank\n",
		     section_name);
	}

	if (!IsSectionSameTypeBankAndFloating(section_name,
					      bank[current_bank].type,
					      current_real_bank)) {
		errx(1, "Different attributes for \"%s\" in source and linkerscript\n",
		     section_name);
	}

	/* Move section to its place. */
	bank[current_bank].address +=
		AssignSectionAddressAndBankByName(section_name,
						  bank[current_bank].address,
						  current_real_bank);
}

