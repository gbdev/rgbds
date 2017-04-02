/*
 * Copyright (C) 2017 Antonio Nino Diaz <antonio_nd@outlook.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include "extern/err.h"
#include "link/assign.h"
#include "link/mylink.h"

static struct {
	unsigned int address; /* current address to write sections to */
	unsigned int top_address; /* not inclusive */
	enum eSectionType type;
} bank[MAXBANKS];

static int current_bank = -1; /* Bank as seen by the bank array */
static int current_real_bank = -1; /* bank as seen by the GB */

void script_InitSections(void)
{
	int i;
	for (i = 0; i < MAXBANKS; i++) {
		if (i == BANK_ROM0) {
			/* ROM0 bank */
			bank[i].address = 0x0000;
			if (options & OPT_TINY) {
				bank[i].top_address = 0x8000;
			} else {
				bank[i].top_address = 0x4000;
			}
			bank[i].type = SECT_ROM0;
		} else if (i >= BANK_ROMX && i < BANK_ROMX + BANK_COUNT_ROMX) {
			/* Swappable ROM bank */
			bank[i].address = 0x4000;
			bank[i].top_address = 0x8000;
			bank[i].type = SECT_ROMX;
		} else if (i == BANK_WRAM0) {
			/* WRAM */
			bank[i].address = 0xC000;
			if (options & OPT_CONTWRAM) {
				bank[i].top_address = 0xE000;
			} else {
				bank[i].top_address = 0xD000;
			}
			bank[i].type = SECT_WRAM0;
		} else if (i >= BANK_SRAM && i < BANK_SRAM + BANK_COUNT_SRAM) {
			/* Swappable SRAM bank */
			bank[i].address = 0xA000;
			bank[i].top_address = 0xC000;
			bank[i].type = SECT_SRAM;
		} else if (i >= BANK_WRAMX && i < BANK_WRAMX + BANK_COUNT_WRAMX) {
			/* Swappable WRAM bank */
			bank[i].address = 0xD000;
			bank[i].top_address = 0xE000;
			bank[i].type = SECT_WRAMX;
		} else if (i >= BANK_VRAM && i < BANK_VRAM + BANK_COUNT_VRAM) {
			/* Swappable VRAM bank */
			bank[i].address = 0x8000;
			bank[i].top_address = 0xA000;
			bank[i].type = SECT_VRAM;
		} else if (i == BANK_OAM) {
			/* OAM */
			bank[i].address = 0xFE00;
			bank[i].top_address = 0xFEA0;
			bank[i].type = SECT_OAM;
		} else if (i == BANK_HRAM) {
			/* HRAM */
			bank[i].address = 0xFF80;
			bank[i].top_address = 0xFFFF;
			bank[i].type = SECT_HRAM;
		} else {
			errx(1, "(INTERNAL) Unknown bank type!");
		}
	}
}

void script_SetCurrentSectionType(const char *type, unsigned int bank)
{
	if (strcmp(type, "ROM0") == 0) {
		if (bank != 0)
			errx(1, "(Internal) Trying to assign a bank number to ROM0.\n");
		current_bank = BANK_ROM0;
		current_real_bank = 0;
		return;
	} else if (strcmp(type, "ROMX") == 0) {
		if (bank == 0)
			errx(1, "ROMX index can't be 0.\n");
		if (bank > BANK_COUNT_ROMX)
			errx(1, "ROMX index too big (%d > %d).\n", bank, BANK_COUNT_ROMX);
		current_bank = BANK_ROMX + bank - 1;
		current_real_bank = bank;
		return;
	} else if (strcmp(type, "VRAM") == 0) {
		if (bank >= BANK_COUNT_VRAM)
			errx(1, "VRAM index too big (%d >= %d).\n", bank, BANK_COUNT_VRAM);
		current_bank = BANK_VRAM + bank;
		current_real_bank = bank;
		return;
	} else if (strcmp(type, "WRAM0") == 0) {
		if (bank != 0)
			errx(1, "(Internal) Trying to assign a bank number to WRAM0.\n");
		current_bank = BANK_WRAM0;
		current_real_bank = 0;
		return;
	} else if (strcmp(type, "WRAMX") == 0) {
		if (bank == 0)
			errx(1, "WRAMX index can't be 0.\n");
		if (bank > BANK_COUNT_WRAMX)
			errx(1, "WRAMX index too big (%d > %d).\n", bank, BANK_COUNT_WRAMX);
		current_bank = BANK_WRAMX + bank - 1;
		current_real_bank = bank - 1;
		return;
	} else if (strcmp(type, "SRAM") == 0) {
		if (bank >= BANK_COUNT_SRAM)
			errx(1, "SRAM index too big (%d >= %d).\n", bank, BANK_COUNT_SRAM);
		current_bank = BANK_SRAM + bank;
		current_real_bank = bank;
		return;
	} else if (strcmp(type, "OAM") == 0) {
		if (bank != 0)
			errx(1, "(Internal) Trying to assign a bank number to OAM.\n");
		current_bank = BANK_OAM;
		current_real_bank = 0;
		return;
	} else if (strcmp(type, "HRAM") == 0) {
		if (bank != 0)
			errx(1, "(Internal) Trying to assign a bank number to HRAM.\n");
		current_bank = BANK_HRAM;
		current_real_bank = 0;
		return;
	}

	errx(1, "(Internal) Unknown section type \"%s\".\n", type);
}

void script_SetAddress(unsigned int addr)
{
	if (current_bank == -1) {
		errx(1, "Trying to set an address without assigned bank\n");
	}

	/* Make sure that we don't go back. */
	if (bank[current_bank].address > addr) {
		errx(1, "Trying to go to a previous address (0x%04X to 0x%04X)\n",
			bank[current_bank].address, addr);
	}

	bank[current_bank].address = addr;

	/* Make sure we don't overflow */
	if (bank[current_bank].address >= bank[current_bank].top_address) {
		errx(1, "Bank overflowed (0x%04X >= 0x%04X)\n",
			bank[current_bank].address, bank[current_bank].top_address);
	}
}

void script_SetAlignment(unsigned int alignment)
{
	if (current_bank == -1) {
		errx(1, "Trying to set an alignment without assigned bank\n");
	}

	if (alignment > 15) {
		errx(1, "Trying to set an alignment too big: %d\n", alignment);
	}

	unsigned int size = 1 << alignment;
	unsigned int mask = size - 1;

	if (bank[current_bank].address & mask) {
		bank[current_bank].address &= ~mask;
		bank[current_bank].address += size;
	}

	/* Make sure we don't overflow */
	if (bank[current_bank].address >= bank[current_bank].top_address) {
		errx(1, "Bank overflowed (0x%04X >= 0x%04X)\n",
			bank[current_bank].address, bank[current_bank].top_address);
	}
}

void script_OutputSection(const char *section_name)
{
	if (current_bank == -1) {
		errx(1, "Trying to place section \"%s\" without assigned bank\n", section_name);
	}

	if (!IsSectionSameTypeBankAndFloating(section_name, bank[current_bank].type,
						current_real_bank)) {
		errx(1, "Different attributes for \"%s\" in source and linkerscript\n",
			section_name);
	}

	/* Move section to its place. */
	bank[current_bank].address +=
		AssignSectionAddressAndBankByName(section_name,
				bank[current_bank].address, current_real_bank);
}

