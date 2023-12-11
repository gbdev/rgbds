/* SPDX-License-Identifier: MIT */

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "link/output.hpp"
#include "link/main.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"

#include "extern/utf8decoder.hpp"

#include "error.hpp"
#include "itertools.hpp"
#include "linkdefs.hpp"
#include "platform.hpp" // For `MIN_NB_ELMS` and `AT`

#define BANK_SIZE 0x4000

FILE *outputFile;
FILE *overlayFile;
FILE *symFile;
FILE *mapFile;

struct SortedSection {
	struct Section const *section;
	struct SortedSection *next;
};

struct SortedSymbol {
	struct Symbol const *sym;
	uint32_t idx;
	uint16_t addr;
};

struct SortedSections {
	struct SortedSection *sections;
	struct SortedSection *zeroLenSections;
};

static struct {
	uint32_t nbBanks; // Size of the array below (which may be NULL if this is 0)
	struct SortedSections *banks;
} sections[SECTTYPE_INVALID];

// Defines the order in which types are output to the sym and map files
static enum SectionType typeMap[SECTTYPE_INVALID] = {
	SECTTYPE_ROM0,
	SECTTYPE_ROMX,
	SECTTYPE_VRAM,
	SECTTYPE_SRAM,
	SECTTYPE_WRAM0,
	SECTTYPE_WRAMX,
	SECTTYPE_OAM,
	SECTTYPE_HRAM
};

void out_AddSection(struct Section const *section)
{
	static uint32_t maxNbBanks[] = {
		AT(SECTTYPE_WRAM0) 1,
		AT(SECTTYPE_VRAM)  2,
		AT(SECTTYPE_ROMX)  UINT32_MAX,
		AT(SECTTYPE_ROM0)  1,
		AT(SECTTYPE_HRAM)  1,
		AT(SECTTYPE_WRAMX) 7,
		AT(SECTTYPE_SRAM)  UINT32_MAX,
		AT(SECTTYPE_OAM)   1,
	};

	uint32_t targetBank = section->bank - sectionTypeInfo[section->type].firstBank;
	uint32_t minNbBanks = targetBank + 1;

	if (minNbBanks > maxNbBanks[section->type])
		errx("Section \"%s\" has an invalid bank range (%" PRIu32 " > %" PRIu32 ")",
		     section->name, section->bank,
		     maxNbBanks[section->type] - 1);

	if (minNbBanks > sections[section->type].nbBanks) {
		sections[section->type].banks =
			(struct SortedSections *)realloc(sections[section->type].banks,
				sizeof(*sections[0].banks) * minNbBanks);
		for (uint32_t i = sections[section->type].nbBanks; i < minNbBanks; i++) {
			sections[section->type].banks[i].sections = NULL;
			sections[section->type].banks[i].zeroLenSections = NULL;
		}
		sections[section->type].nbBanks = minNbBanks;
	}
	if (!sections[section->type].banks)
		err("Failed to realloc banks");

	struct SortedSection *newSection = (struct SortedSection *)malloc(sizeof(*newSection));
	struct SortedSection **ptr = section->size
		? &sections[section->type].banks[targetBank].sections
		: &sections[section->type].banks[targetBank].zeroLenSections;

	if (!newSection)
		err("Failed to add new section \"%s\"", section->name);
	newSection->section = section;

	while (*ptr && (*ptr)->section->org < section->org)
		ptr = &(*ptr)->next;

	newSection->next = *ptr;
	*ptr = newSection;
}

struct Section const *out_OverlappingSection(struct Section const *section)
{
	struct SortedSections *banks = sections[section->type].banks;
	struct SortedSection *ptr =
		banks[section->bank - sectionTypeInfo[section->type].firstBank].sections;

	while (ptr) {
		if (ptr->section->org < section->org + section->size
		 && section->org < ptr->section->org + ptr->section->size)
			return ptr->section;
		ptr = ptr->next;
	}
	return NULL;
}

/*
 * Performs sanity checks on the overlay file.
 * @return The number of ROM banks in the overlay file
 */
static uint32_t checkOverlaySize(void)
{
	if (!overlayFile)
		return 0;

	if (fseek(overlayFile, 0, SEEK_END) != 0) {
		warnx("Overlay file is not seekable, cannot check if properly formed");
		return 0;
	}

	long overlaySize = ftell(overlayFile);

	// Reset back to beginning
	fseek(overlayFile, 0, SEEK_SET);

	if (overlaySize % BANK_SIZE)
		errx("Overlay file must have a size multiple of 0x4000");

	uint32_t nbOverlayBanks = overlaySize / BANK_SIZE;

	if (is32kMode && nbOverlayBanks != 2)
		errx("Overlay must be exactly 0x8000 bytes large");

	if (nbOverlayBanks < 2)
		errx("Overlay must be at least 0x8000 bytes large");

	return nbOverlayBanks;
}

/*
 * Expand sections[SECTTYPE_ROMX].banks to cover all the overlay banks.
 * This ensures that writeROM will output each bank, even if some are not
 * covered by any sections.
 * @param nbOverlayBanks The number of banks in the overlay file
 */
static void coverOverlayBanks(uint32_t nbOverlayBanks)
{
	// 2 if is32kMode, 1 otherwise
	uint32_t nbRom0Banks = sectionTypeInfo[SECTTYPE_ROM0].size / BANK_SIZE;
	// Discount ROM0 banks to avoid outputting too much
	uint32_t nbUncoveredBanks = nbOverlayBanks - nbRom0Banks > sections[SECTTYPE_ROMX].nbBanks
				    ? nbOverlayBanks - nbRom0Banks
				    : 0;

	if (nbUncoveredBanks > sections[SECTTYPE_ROMX].nbBanks) {
		sections[SECTTYPE_ROMX].banks =
			(struct SortedSections *)realloc(sections[SECTTYPE_ROMX].banks,
				sizeof(*sections[SECTTYPE_ROMX].banks) * nbUncoveredBanks);
		if (!sections[SECTTYPE_ROMX].banks)
			err("Failed to realloc banks for overlay");
		for (uint32_t i = sections[SECTTYPE_ROMX].nbBanks; i < nbUncoveredBanks; i++) {
			sections[SECTTYPE_ROMX].banks[i].sections = NULL;
			sections[SECTTYPE_ROMX].banks[i].zeroLenSections = NULL;
		}
		sections[SECTTYPE_ROMX].nbBanks = nbUncoveredBanks;
	}
}

/*
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

		assert(section->offset == 0);
		// Output padding up to the next SECTION
		while (offset + baseOffset < section->org) {
			putc(overlayFile ? getc(overlayFile) : padValue, outputFile);
			offset++;
		}

		// Output the section itself
		fwrite(section->data, sizeof(*section->data), section->size,
		       outputFile);
		if (overlayFile) {
			// Skip bytes even with pipes
			for (uint16_t i = 0; i < section->size; i++)
				getc(overlayFile);
		}
		offset += section->size;

		bankSections = bankSections->next;
	}

	if (!disablePadding) {
		while (offset < size) {
			putc(overlayFile ? getc(overlayFile) : padValue, outputFile);
			offset++;
		}
	}
}

// Writes a ROM file to the output.
static void writeROM(void)
{
	outputFile = openFile(outputFileName, "wb");
	overlayFile = openFile(overlayFileName, "rb");

	uint32_t nbOverlayBanks = checkOverlaySize();

	if (nbOverlayBanks > 0)
		coverOverlayBanks(nbOverlayBanks);

	if (outputFile) {
		writeBank(sections[SECTTYPE_ROM0].banks ? sections[SECTTYPE_ROM0].banks[0].sections
							: NULL,
			  sectionTypeInfo[SECTTYPE_ROM0].startAddr, sectionTypeInfo[SECTTYPE_ROM0].size);

		for (uint32_t i = 0 ; i < sections[SECTTYPE_ROMX].nbBanks; i++)
			writeBank(sections[SECTTYPE_ROMX].banks[i].sections,
				  sectionTypeInfo[SECTTYPE_ROMX].startAddr, sectionTypeInfo[SECTTYPE_ROMX].size);
	}

	closeFile(outputFile);
	closeFile(overlayFile);
}

/*
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

// Checks whether this character is legal as the first character of a symbol's name in a sym file
static bool canStartSymName(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

// Checks whether this character is legal in a symbol's name in a sym file
static bool isLegalForSymName(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	       c == '_' || c == '@' || c == '#' || c == '$' || c == '.';
}

// Prints a symbol's name to `symFile`, assuming that the first character is legal.
// Illegal characters are UTF-8-decoded (errors are replaced by U+FFFD) and emitted as `\u`/`\U`.
static void printSymName(char const *name)
{
	for (char const *ptr = name; *ptr != '\0'; ) {
		char c = *ptr;

		if (isLegalForSymName(c)) {
			// Output legal ASCII characters as-is
			putc(c, symFile);
			++ptr;
		} else {
			// Output illegal characters using Unicode escapes
			// Decode the UTF-8 codepoint; or at least attempt to
			uint32_t state = 0, codepoint;

			do {
				decode(&state, &codepoint, *ptr);
				if (state == 1) {
					// This sequence was invalid; emit a U+FFFD, and recover
					codepoint = 0xFFFD;
					// Skip continuation bytes
					// A NUL byte does not qualify, so we're good
					while ((*ptr & 0xC0) == 0x80)
						++ptr;
					break;
				}
				++ptr;
			} while (state != 0);

			fprintf(symFile, codepoint <= 0xFFFF ? "\\u%04" PRIx32 : "\\U%08" PRIx32,
				codepoint);
		}
	}
}

// Comparator function for `qsort` to sort symbols
// Symbols are ordered by address, then by parentage, or else by original index for a stable sort
static int compareSymbols(void const *a, void const *b)
{
	struct SortedSymbol const *sym1 = (struct SortedSymbol const *)a;
	struct SortedSymbol const *sym2 = (struct SortedSymbol const *)b;

	if (sym1->addr != sym2->addr)
		return sym1->addr < sym2->addr ? -1 : 1;

	char const *sym1_name = sym1->sym->name;
	char const *sym2_name = sym2->sym->name;
	bool sym1_local = strchr(sym1_name, '.');
	bool sym2_local = strchr(sym2_name, '.');

	if (sym1_local != sym2_local) {
		size_t sym1_len = strlen(sym1_name);
		size_t sym2_len = strlen(sym2_name);

		// Sort parent labels before their child local labels
		if (!strncmp(sym1_name, sym2_name, sym1_len) && sym2_name[sym1_len] == '.')
			return -1;
		if (!strncmp(sym2_name, sym1_name, sym2_len) && sym1_name[sym2_len] == '.')
			return 1;
		// Sort local labels before unrelated global labels
		return sym1_local ? -1 : 1;
	}

	return sym1->idx < sym2->idx ? -1 : sym1->idx > sym2->idx ? 1 : 0;
}

/*
 * Write a bank's contents to the sym file
 * @param bankSections The bank's sections
 */
static void writeSymBank(struct SortedSections const *bankSections,
			 enum SectionType type, uint32_t bank)
{
#define forEachSortedSection(sect, ...) do { \
	for (struct SortedSection const *ssp = bankSections->zeroLenSections; ssp; ssp = ssp->next) { \
		for (struct Section const *sect = ssp->section; sect; sect = sect->nextu) \
			__VA_ARGS__ \
	} \
	for (struct SortedSection const *ssp = bankSections->sections; ssp; ssp = ssp->next) { \
		for (struct Section const *sect = ssp->section; sect; sect = sect->nextu) \
			__VA_ARGS__ \
	} \
} while (0)

	uint32_t nbSymbols = 0;

	forEachSortedSection(sect, {
		nbSymbols += sect->nbSymbols;
	});

	if (!nbSymbols)
		return;

	struct SortedSymbol *symList = (struct SortedSymbol *)malloc(sizeof(*symList) * nbSymbols);

	if (!symList)
		err("Failed to allocate symbol list");

	nbSymbols = 0;

	forEachSortedSection(sect, {
		for (uint32_t i = 0; i < sect->nbSymbols; i++) {
			if (!canStartSymName(sect->symbols[i]->name[0]))
				// Don't output symbols that begin with an illegal character
				continue;
			symList[nbSymbols].idx = nbSymbols;
			symList[nbSymbols].sym = sect->symbols[i];
			symList[nbSymbols].addr = symList[nbSymbols].sym->offset + sect->org;
			nbSymbols++;
		}
	});

#undef forEachSortedSection

	qsort(symList, nbSymbols, sizeof(*symList), compareSymbols);

	uint32_t symBank = bank + sectionTypeInfo[type].firstBank;

	for (uint32_t i = 0; i < nbSymbols; i++) {
		struct SortedSymbol *sym = &symList[i];

		fprintf(symFile, "%02" PRIx32 ":%04" PRIx16 " ", symBank, sym->addr);
		printSymName(sym->sym->name);
		putc('\n', symFile);
	}

	free(symList);

}

static void writeEmptySpace(uint16_t begin, uint16_t end)
{
	if (begin < end) {
		uint16_t len = end - begin;

		fprintf(mapFile, "\tEMPTY: $%04x-$%04x ($%04" PRIx16 " byte%s)\n",
			begin, end - 1, len, len == 1 ? "" : "s");
	}
}

/*
 * Write a bank's contents to the map file
 */
static void writeMapBank(struct SortedSections const *sectList, enum SectionType type,
			 uint32_t bank)
{
	fprintf(mapFile, "\n%s bank #%" PRIu32 ":\n", sectionTypeInfo[type].name.c_str(),
		bank + sectionTypeInfo[type].firstBank);

	uint16_t used = 0;
	struct SortedSection const *section = sectList->sections;
	struct SortedSection const *zeroLenSection = sectList->zeroLenSections;
	uint16_t prevEndAddr = sectionTypeInfo[type].startAddr;

	while (section || zeroLenSection) {
		struct SortedSection const **pickedSection =
			nextSection(&section, &zeroLenSection);
		struct Section const *sect = (*pickedSection)->section;

		used += sect->size;
		assert(sect->offset == 0);

		writeEmptySpace(prevEndAddr, sect->org);

		prevEndAddr = sect->org + sect->size;

		if (sect->size != 0)
			fprintf(mapFile, "\tSECTION: $%04" PRIx16 "-$%04x ($%04" PRIx16
				" byte%s) [\"%s\"]\n",
				sect->org, prevEndAddr - 1,
				sect->size, sect->size == 1 ? "" : "s",
				sect->name);
		else
			fprintf(mapFile, "\tSECTION: $%04" PRIx16 " (0 bytes) [\"%s\"]\n",
				sect->org, sect->name);

		if (!noSymInMap) {
			uint16_t org = sect->org;

			while (sect) {
				for (size_t i = 0; i < sect->nbSymbols; i++)
					// Space matches "\tSECTION: $xxxx ..."
					fprintf(mapFile, "\t         $%04" PRIx32 " = %s\n",
						sect->symbols[i]->offset + org,
						sect->symbols[i]->name);

				if (sect->nextu) {
					// Announce the following "piece"
					if (sect->nextu->modifier == SECTION_UNION)
						fprintf(mapFile,
							"\t         ; Next union\n");
					else if (sect->nextu->modifier == SECTION_FRAGMENT)
						fprintf(mapFile,
							"\t         ; Next fragment\n");
				}

				sect = sect->nextu; // Also print symbols in the following "pieces"
			}
		}

		*pickedSection = (*pickedSection)->next;
	}

	if (used == 0) {
		fputs("\tEMPTY\n", mapFile);
	} else {
		uint16_t bankEndAddr = sectionTypeInfo[type].startAddr + sectionTypeInfo[type].size;

		writeEmptySpace(prevEndAddr, bankEndAddr);

		uint16_t slack = sectionTypeInfo[type].size - used;

		fprintf(mapFile, "\tTOTAL EMPTY: $%04" PRIx16 " byte%s\n", slack,
			slack == 1 ? "" : "s");
	}
}

/*
 * Write the total used and free space by section type to the map file
 */
static void writeMapSummary(void)
{
	fputs("SUMMARY:\n", mapFile);

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		enum SectionType type = typeMap[i];
		uint32_t nbBanks = sections[type].nbBanks;

		// Do not output used space for VRAM or OAM
		if (type == SECTTYPE_VRAM || type == SECTTYPE_OAM)
			continue;

		// Do not output unused section types
		if (nbBanks == 0)
			continue;

		uint32_t usedTotal = 0;

		for (uint32_t bank = 0; bank < nbBanks; bank++) {
			uint16_t used = 0;
			struct SortedSections const *sectList = &sections[type].banks[bank];
			struct SortedSection const *section = sectList->sections;
			struct SortedSection const *zeroLenSection = sectList->zeroLenSections;

			while (section || zeroLenSection) {
				struct SortedSection const **pickedSection =
					nextSection(&section, &zeroLenSection);

				used += (*pickedSection)->section->size;
				*pickedSection = (*pickedSection)->next;
			}

			usedTotal += used;
		}

		fprintf(mapFile, "\t%s: %" PRId32 " byte%s used / %" PRId32 " free",
			sectionTypeInfo[type].name.c_str(), usedTotal, usedTotal == 1 ? "" : "s",
			nbBanks * sectionTypeInfo[type].size - usedTotal);
		if (sectionTypeInfo[type].firstBank != sectionTypeInfo[type].lastBank
		    || nbBanks > 1)
			fprintf(mapFile, " in %d bank%s", nbBanks, nbBanks == 1 ? "" : "s");
		putc('\n', mapFile);
	}
}

// Writes the sym file, if applicable.
static void writeSym(void)
{
	if (!symFileName)
		return;

	symFile = openFile(symFileName, "w");
	if (!symFile)
		err("Failed to open sym file \"%s\"", symFileName);

	fputs("; File generated by rgblink\n", symFile);

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		enum SectionType type = typeMap[i];

		for (uint32_t bank = 0; bank < sections[type].nbBanks; bank++)
			writeSymBank(&sections[type].banks[bank], type, bank);
	}

	closeFile(symFile);
}

// Writes the map file, if applicable.
static void writeMap(void)
{
	if (!mapFileName)
		return;

	mapFile = openFile(mapFileName, "w");
	if (!mapFile)
		err("Failed to open map file \"%s\"", mapFileName);

	writeMapSummary();

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		enum SectionType type = typeMap[i];

		for (uint32_t bank = 0; bank < sections[type].nbBanks; bank++)
			writeMapBank(&sections[type].banks[bank], type, bank);
	}

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
	for (enum SectionType type : EnumSeq(SECTTYPE_INVALID)) {
		for (uint32_t i = 0; i < sections[type].nbBanks; i++) {
			struct SortedSections *bank = &sections[type].banks[i];

			cleanupSections(bank->sections);
			cleanupSections(bank->zeroLenSections);
		}
		free(sections[type].banks);
	}
}

void out_WriteFiles(void)
{
	writeROM();
	writeSym();
	writeMap();

	cleanup();
}
