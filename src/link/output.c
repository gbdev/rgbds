/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2019, Eldred Habert and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <stdint.h>

#include "link/output.h"
#include "link/main.h"
#include "link/section.h"
#include "link/symbol.h"

#include "extern/err.h"

FILE * outputFile;
FILE *overlayFile;
FILE *symFile;
FILE *mapFile;

struct SortedSection {
	struct Section const *section;
	struct SortedSection *next;
};

static struct {
	uint32_t nbBanks;
	struct SortedSections {
		struct SortedSection *sections;
		struct SortedSection *zeroLenSections;
	} *banks;
} sections[SECTTYPE_INVALID];

void out_AddSection(struct Section const *section)
{
	static uint32_t maxNbBanks[] = {
		[SECTTYPE_ROM0]  = 1,
		[SECTTYPE_ROMX]  = UINT32_MAX,
		[SECTTYPE_VRAM]  = 2,
		[SECTTYPE_SRAM]  = UINT32_MAX,
		[SECTTYPE_WRAM0] = 1,
		[SECTTYPE_WRAMX] = 7,
		[SECTTYPE_OAM]   = 1,
		[SECTTYPE_HRAM]  = 1
	};

	uint32_t targetBank = section->bank - bankranges[section->type][0];
	uint32_t minNbBanks = targetBank + 1;

	if (minNbBanks > maxNbBanks[section->type])
		errx(1, "Section \"%s\" has invalid bank range (%u > %u)",
		     section->name, section->bank,
		     maxNbBanks[section->type] - 1);

	if (minNbBanks > sections[section->type].nbBanks) {
		sections[section->type].banks =
			realloc(sections[section->type].banks,
				sizeof(*sections[0].banks) * minNbBanks);
		for (uint32_t i = sections[section->type].nbBanks;
		     i < minNbBanks; i++) {
			sections[section->type].banks[i].sections = NULL;
			sections[section->type].banks[i].zeroLenSections = NULL;
		}
		sections[section->type].nbBanks = minNbBanks;
	}
	if (!sections[section->type].banks)
		err(1, "Failed to realloc banks");

	struct SortedSection *newSection = malloc(sizeof(*newSection));
	struct SortedSection **ptr = section->size
		? &sections[section->type].banks[targetBank].sections
		: &sections[section->type].banks[targetBank].zeroLenSections;

	if (!newSection)
		err(1, "Failed to add new section \"%s\"", section->name);
	newSection->section = section;

	while (*ptr && (*ptr)->section->org < section->org)
		ptr = &(*ptr)->next;

	newSection->next = *ptr;
	*ptr = newSection;
}

struct Section const *out_OverlappingSection(struct Section const *section)
{
	struct SortedSection *ptr =
		sections[section->type].banks[section->bank].sections;

	while (ptr) {
		if (ptr->section->org < section->org + section->size
		 && section->org < ptr->section->org + ptr->section->size)
			return ptr->section;
		ptr = ptr->next;
	}
	return NULL;
}

/**
 * Performs sanity checks on the overlay file.
 */
static void checkOverlay(void)
{
	if (!overlayFile)
		return;

	if (fseek(overlayFile, 0, SEEK_END) != 0) {
		warnx("Overlay file is not seekable, cannot check if properly formed");
		return;
	}

	long overlaySize = ftell(overlayFile);

	if (overlaySize % 0x4000)
		errx(1, "Overlay file must have a size multiple of 0x4000");

	/* Reset back to beginning */
	fseek(overlayFile, 0, SEEK_SET);

	uint32_t nbOverlayBanks = overlaySize / 0x4000 - 1;

	if (nbOverlayBanks < 1)
		errx(1, "Overlay must be at least 0x8000 bytes large");

	if (nbOverlayBanks > sections[SECTTYPE_ROMX].nbBanks) {
		sections[SECTTYPE_ROMX].banks =
			realloc(sections[SECTTYPE_ROMX].banks,
				sizeof(*sections[SECTTYPE_ROMX].banks) *
					nbOverlayBanks);
		if (!sections[SECTTYPE_ROMX].banks)
			err(1, "Failed to realloc banks for overlay");
		for (uint32_t i = sections[SECTTYPE_ROMX].nbBanks;
		     i < nbOverlayBanks; i++) {
			sections[SECTTYPE_ROMX].banks[i].sections = NULL;
			sections[SECTTYPE_ROMX].banks[i].zeroLenSections = NULL;
		}
		sections[SECTTYPE_ROMX].nbBanks = nbOverlayBanks;
	}
}

/**
 * Write a ROM bank's sections to the output file.
 * @param bankSections The bank's sections, ordered by increasing address
 * @param baseOffset The address of the bank's first byte in GB address space
 * @param size The size of the bank
 */
static void writeBank(struct SortedSection *bankSections, uint16_t baseOffset,
		      uint16_t size)
{
	uint16_t offset = 0;

	while (bankSections) {
		struct Section const *section = bankSections->section;

		/* Output padding up to the next SECTION */
		while (offset + baseOffset < section->org) {
			putc(overlayFile ? getc(overlayFile) : padValue,
			     outputFile);
			offset++;
		}

		/* Output the section itself */
		fwrite(section->data, sizeof(*section->data), section->size,
		       outputFile);
		if (overlayFile) {
			/* Skip bytes even with pipes */
			for (uint16_t i = 0; i < section->size; i++)
				getc(overlayFile);
		}
		offset += section->size;

		bankSections = bankSections->next;
	}

	while (offset < size) {
		putc(overlayFile ? getc(overlayFile) : padValue, outputFile);
		offset++;
	}
}

/**
 * Writes a ROM file to the output.
 */
static void writeROM(void)
{
	outputFile = openFile(outputFileName, "wb");
	overlayFile = openFile(overlayFileName, "rb");

	checkOverlay();

	if (outputFile) {
		if (sections[SECTTYPE_ROM0].nbBanks > 0)
			writeBank(sections[SECTTYPE_ROM0].banks[0].sections,
				  0x0000, 0x4000);

		for (uint32_t i = 0 ; i < sections[SECTTYPE_ROMX].nbBanks; i++)
			writeBank(sections[SECTTYPE_ROMX].banks[i].sections,
				  0x4000, 0x4000);
	}

	closeFile(outputFile);
	closeFile(overlayFile);
}

/**
 * Get the lowest section by address out of the two
 * @param s1 One choice
 * @param s2 The other
 * @return The lowest section of the two, or the non-NULL one if applicable
 */
static struct SortedSection const **nextSection(struct SortedSection const **s1,
						struct SortedSection const **s2)
{
	if (!*s1)
		return s2;
	if (!*s2)
		return s1;

	return (*s1)->section->org < (*s2)->section->org ? s1 : s2;
}

/**
 * Write a bank's contents to the sym file
 * @param bankSections The bank's sections
 */
static void writeSymBank(struct SortedSections const *bankSections)
{
	if (!symFile)
		return;

	struct {
		struct SortedSection const *sections;
#define sect sections->section /* Fake member as a shortcut */
		uint32_t i;
		struct Symbol const *sym;
		uint16_t addr;
	} sectList = { .sections = bankSections->sections, .i = 0 },
	zlSectList = { .sections = bankSections->zeroLenSections, .i = 0 },
	  *minSectList;

	for (;;) {
		while (sectList.sections
		    && sectList.i   == sectList.sect->nbSymbols) {
			sectList.sections   = sectList.sections->next;
			sectList.i   = 0;
		}
		while (zlSectList.sections
		    && zlSectList.i == zlSectList.sect->nbSymbols) {
			zlSectList.sections = zlSectList.sections->next;
			zlSectList.i = 0;
		}

		if (!sectList.sections && !zlSectList.sections) {
			break;
		} else if (sectList.sections && zlSectList.sections) {
			sectList.sym   = sectList.sect->symbols[sectList.i];
			zlSectList.sym = zlSectList.sect->symbols[zlSectList.i];
			sectList.addr =
				sectList.sym->offset   + sectList.sect->org;
			zlSectList.addr =
				zlSectList.sym->offset + zlSectList.sect->org;

			minSectList = sectList.addr < zlSectList.addr
								? &sectList
								: &zlSectList;
		} else if (sectList.sections) {
			sectList.sym   = sectList.sect->symbols[sectList.i];
			sectList.addr   =
				sectList.sym->offset   + sectList.sect->org;

			minSectList = &sectList;
		} else {
			zlSectList.sym = zlSectList.sect->symbols[zlSectList.i];
			zlSectList.addr =
				zlSectList.sym->offset + zlSectList.sect->org;

			minSectList = &zlSectList;
		}
		fprintf(symFile, "%02x:%04x %s\n",
			minSectList->sect->bank, minSectList->addr,
			minSectList->sym->name);
		minSectList->i++;
	}
#undef sect
}

/**
 * Write a bank's contents to the map file
 * @param bankSections The bank's sections
 */
static void writeMapBank(struct SortedSections const *sectList,
			 enum SectionType type, uint32_t bank)
{
	if (!mapFile)
		return;

	struct SortedSection const *section        = sectList->sections;
	struct SortedSection const *zeroLenSection = sectList->zeroLenSections;

	fprintf(mapFile, "%s bank #%u:\n", typeNames[type],
		bank + bankranges[type][0]);

	uint16_t slack = maxsize[type];

	while (section || zeroLenSection) {
		struct SortedSection const **pickedSection =
			nextSection(&section, &zeroLenSection);
		struct Section const *sect = (*pickedSection)->section;

		slack -= sect->size;

		fprintf(mapFile, "  SECTION: $%04x-$%04x ($%04x byte%s) [\"%s\"]\n",
			sect->org, sect->org + sect->size - 1, sect->size,
			sect->size == 1 ? "" : "s", sect->name);

		for (size_t i = 0; i < sect->nbSymbols; i++)
			fprintf(mapFile, "           $%04x = %s\n",
				sect->symbols[i]->offset + sect->org,
				sect->symbols[i]->name);

		*pickedSection = (*pickedSection)->next;
	}

	if (slack == maxsize[type])
		fputs("  EMPTY\n\n", mapFile);
	else
		fprintf(mapFile, "    SLACK: $%04x byte%s\n\n", slack,
			slack == 1 ? "" : "s");
}

/**
 * Writes the sym and/or map files, if applicable.
 */
static void writeSymAndMap(void)
{
	if (!symFileName && !mapFileName)
		return;

	enum SectionType typeMap[SECTTYPE_INVALID] = {
		SECTTYPE_ROM0,
		SECTTYPE_ROMX,
		SECTTYPE_VRAM,
		SECTTYPE_SRAM,
		SECTTYPE_WRAM0,
		SECTTYPE_WRAMX,
		SECTTYPE_OAM,
		SECTTYPE_HRAM
	};

	symFile = openFile(symFileName, "w");
	mapFile = openFile(mapFileName, "w");

	if (symFileName)
		fputs("; File generated by rgblink\n", symFile);

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		enum SectionType type = typeMap[i];

		if (sections[type].nbBanks > 0) {
			for (uint32_t bank = 0; bank < sections[type].nbBanks;
			     bank++) {
				writeSymBank(&sections[type].banks[bank]);
				writeMapBank(&sections[type].banks[bank],
					     type, bank);
			}
		}
	}

	closeFile(symFile);
	closeFile(mapFile);
}

static void cleanupSections(struct SortedSection *section)
{
	while (section) {
		struct SortedSection *next = section->next;

		free(section);
		section = next;
	}
}

static void cleanup(void)
{
	for (enum SectionType type = 0; type < SECTTYPE_INVALID; type++) {
		if (sections[type].nbBanks > 0) {
			for (uint32_t i = 0; i < sections[type].nbBanks; i++) {
				struct SortedSections *bank =
					&sections[type].banks[i];

				cleanupSections(bank->sections);
				cleanupSections(bank->zeroLenSections);
			}
			free(sections[type].banks);
		}
	}
}

void out_WriteFiles(void)
{
	writeROM();
	writeSymAndMap();

	cleanup();
}
