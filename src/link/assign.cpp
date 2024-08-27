/* SPDX-License-Identifier: MIT */

#include "link/assign.hpp"

#include <deque>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "error.hpp"
#include "helpers.hpp"
#include "itertools.hpp"
#include "linkdefs.hpp"
#include "platform.hpp"

#include "link/main.hpp"
#include "link/output.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"

struct MemoryLocation {
	uint16_t address;
	uint32_t bank;
};

struct FreeSpace {
	uint16_t address;
	uint16_t size;
};

// Table of free space for each bank
std::vector<std::deque<FreeSpace>> memory[SECTTYPE_INVALID];

uint64_t nbSectionsToAssign;

// Init the free space-modelling structs
static void initFreeSpace() {
	for (SectionType type : EnumSeq(SECTTYPE_INVALID)) {
		memory[type].resize(nbbanks(type));
		for (std::deque<FreeSpace> &bankMem : memory[type]) {
			bankMem.push_back({
			    .address = sectionTypeInfo[type].startAddr,
			    .size = sectionTypeInfo[type].size,
			});
		}
	}
}

/*
 * Assigns a section to a given memory location
 * @param section The section to assign
 * @param location The location to assign the section to
 */
static void assignSection(Section &section, MemoryLocation const &location) {
	// Propagate the assigned location to all UNIONs/FRAGMENTs
	// so `jr` patches in them will have the correct offset
	for (Section *next = &section; next != nullptr; next = next->nextu.get()) {
		next->org = location.address;
		next->bank = location.bank;
	}

	nbSectionsToAssign--;

	out_AddSection(section);
}

/*
 * Checks whether a given location is suitable for placing a given section
 * This checks not only that the location has enough room for the section, but
 * also that the constraints (alignment...) are respected.
 * @param section The section to be placed
 * @param freeSpace The candidate free space to place the section into
 * @param location The location to attempt placing the section at
 * @return True if the location is suitable, false otherwise.
 */
static bool isLocationSuitable(
    Section const &section, FreeSpace const &freeSpace, MemoryLocation const &location
) {
	if (section.isAddressFixed && section.org != location.address)
		return false;

	if (section.isAlignFixed && ((location.address - section.alignOfs) & section.alignMask))
		return false;

	if (location.address < freeSpace.address)
		return false;

	return location.address + section.size <= freeSpace.address + freeSpace.size;
}

/*
 * Finds a suitable location to place a section at.
 * @param section The section to be placed
 * @param location A pointer to a memory location that will be filled
 * @return The index into `memory[section->type]` of the free space encompassing the location,
 *         or -1 if none was found
 */
static ssize_t getPlacement(Section const &section, MemoryLocation &location) {
	SectionTypeInfo const &typeInfo = sectionTypeInfo[section.type];

	static uint16_t curScrambleROM = 0;
	static uint8_t curScrambleWRAM = 0;
	static int8_t curScrambleSRAM = 0;

	// Determine which bank we should start searching in
	if (section.isBankFixed) {
		location.bank = section.bank;
	} else if (scrambleROMX && section.type == SECTTYPE_ROMX) {
		if (curScrambleROM < 1)
			curScrambleROM = scrambleROMX;
		location.bank = curScrambleROM--;
	} else if (scrambleWRAMX && section.type == SECTTYPE_WRAMX) {
		if (curScrambleWRAM < 1)
			curScrambleWRAM = scrambleWRAMX;
		location.bank = curScrambleWRAM--;
	} else if (scrambleSRAM && section.type == SECTTYPE_SRAM) {
		if (curScrambleSRAM < 0)
			curScrambleSRAM = scrambleSRAM;
		location.bank = curScrambleSRAM--;
	} else {
		location.bank = typeInfo.firstBank;
	}

	for (;;) {
		// Switch to the beginning of the next bank
		std::deque<FreeSpace> &bankMem = memory[section.type][location.bank - typeInfo.firstBank];
		size_t spaceIdx = 0;

		if (spaceIdx < bankMem.size()) {
			location.address = bankMem[spaceIdx].address;

			// Process locations in that bank
			while (spaceIdx < bankMem.size()) {
				// If that location is OK, return it
				if (isLocationSuitable(section, bankMem[spaceIdx], location))
					return spaceIdx;

				// Go to the next *possible* location
				if (section.isAddressFixed) {
					// If the address is fixed, there can be only
					// one candidate block per bank; if we already
					// reached it, give up.
					if (location.address < section.org)
						location.address = section.org;
					else
						break; // Try again in next bank
				} else if (section.isAlignFixed) {
					// Move to next aligned location
					// Move back to alignment boundary
					location.address -= section.alignOfs;
					// Ensure we're there (e.g. on first check)
					location.address &= ~section.alignMask;
					// Go to next align boundary and add offset
					location.address += section.alignMask + 1 + section.alignOfs;
				} else {
					// Any location is fine, so, next free block
					spaceIdx++;
					if (spaceIdx < bankMem.size())
						location.address = bankMem[spaceIdx].address;
				}

				// If that location is past the current block's end,
				// go forwards until that is no longer the case.
				while (spaceIdx < bankMem.size()
				       && location.address >= bankMem[spaceIdx].address + bankMem[spaceIdx].size)
					spaceIdx++;

				// Try again with the new location/free space combo
			}
		}

		// Try again in the next bank, if one is available.
		// Try scrambled banks in descending order until no bank in the scrambled range is
		// available. Otherwise, try in ascending order.
		if (section.isBankFixed) {
			return -1;
		} else if (scrambleROMX && section.type == SECTTYPE_ROMX && location.bank <= scrambleROMX) {
			if (location.bank > typeInfo.firstBank)
				location.bank--;
			else if (scrambleROMX < typeInfo.lastBank)
				location.bank = scrambleROMX + 1;
			else
				return -1;
		} else if (scrambleWRAMX && section.type == SECTTYPE_WRAMX && location.bank <= scrambleWRAMX) {
			if (location.bank > typeInfo.firstBank)
				location.bank--;
			else if (scrambleWRAMX < typeInfo.lastBank)
				location.bank = scrambleWRAMX + 1;
			else
				return -1;
		} else if (scrambleSRAM && section.type == SECTTYPE_SRAM && location.bank <= scrambleSRAM) {
			if (location.bank > typeInfo.firstBank)
				location.bank--;
			else if (scrambleSRAM < typeInfo.lastBank)
				location.bank = scrambleSRAM + 1;
			else
				return -1;
		} else if (location.bank < typeInfo.lastBank) {
			location.bank++;
		} else {
			return -1;
		}
	}
}

/*
 * Places a section in a suitable location, or error out if it fails to.
 * @warning Due to the implemented algorithm, this should be called with
 *          sections of decreasing size.
 * @param section The section to place
 */
static void placeSection(Section &section) {
	MemoryLocation location;

	// Specially handle 0-byte SECTIONs, as they can't overlap anything
	if (section.size == 0) {
		// Unless the SECTION's address was fixed, the starting address
		// is fine for any alignment, as checked in sect_DoSanityChecks.
		location.address =
		    section.isAddressFixed ? section.org : sectionTypeInfo[section.type].startAddr;
		location.bank =
		    section.isBankFixed ? section.bank : sectionTypeInfo[section.type].firstBank;
		assignSection(section, location);
		return;
	}

	// Place section using first-fit decreasing algorithm
	// https://en.wikipedia.org/wiki/Bin_packing_problem#First-fit_algorithm
	if (ssize_t spaceIdx = getPlacement(section, location); spaceIdx != -1) {
		std::deque<FreeSpace> &bankMem =
		    memory[section.type][location.bank - sectionTypeInfo[section.type].firstBank];
		FreeSpace &freeSpace = bankMem[spaceIdx];

		assignSection(section, location);

		// Update the free space
		uint16_t sectionEnd = section.org + section.size;
		bool noLeftSpace = freeSpace.address == section.org;
		bool noRightSpace = freeSpace.address + freeSpace.size == sectionEnd;
		if (noLeftSpace && noRightSpace) {
			// The free space is entirely deleted
			bankMem.erase(bankMem.begin() + spaceIdx);
		} else if (!noLeftSpace && !noRightSpace) {
			// The free space is split in two
			// Append the new space after the original one
			bankMem.insert(
			    bankMem.begin() + spaceIdx + 1,
			    {.address = sectionEnd,
			     .size = (uint16_t)(freeSpace.address + freeSpace.size - sectionEnd)}
			);
			// **`freeSpace` cannot be reused from this point on, because `bankMem.insert`
			// invalidates all references to itself!**

			// Resize the original space (address is unmodified)
			bankMem[spaceIdx].size = section.org - bankMem[spaceIdx].address;
		} else {
			// The amount of free spaces doesn't change: resize!
			freeSpace.size -= section.size;
			if (noLeftSpace)
				// The free space is moved *and* resized
				freeSpace.address += section.size;
		}
		return;
	}

	// Please adjust depending on longest message below
	char where[64];

	if (section.isBankFixed && nbbanks(section.type) != 1) {
		if (section.isAddressFixed)
			snprintf(
			    where, sizeof(where), "at $%02" PRIx32 ":%04" PRIx16, section.bank, section.org
			);
		else if (section.isAlignFixed)
			snprintf(
			    where,
			    sizeof(where),
			    "in bank $%02" PRIx32 " with align mask $%" PRIx16,
			    section.bank,
			    (uint16_t)~section.alignMask
			);
		else
			snprintf(where, sizeof(where), "in bank $%02" PRIx32, section.bank);
	} else {
		if (section.isAddressFixed)
			snprintf(where, sizeof(where), "at address $%04" PRIx16, section.org);
		else if (section.isAlignFixed)
			snprintf(
			    where,
			    sizeof(where),
			    "with align mask $%" PRIx16 " and offset $%" PRIx16,
			    (uint16_t)~section.alignMask,
			    section.alignOfs
			);
		else
			strcpy(where, "anywhere");
	}

	// If a section failed to go to several places, nothing we can report
	if (!section.isBankFixed || !section.isAddressFixed)
		errx(
		    "Unable to place \"%s\" (%s section) %s",
		    section.name.c_str(),
		    sectionTypeInfo[section.type].name.c_str(),
		    where
		);
	// If the section just can't fit the bank, report that
	else if (section.org + section.size > endaddr(section.type) + 1)
		errx(
		    "Unable to place \"%s\" (%s section) %s: section runs past end of region ($%04x > "
		    "$%04x)",
		    section.name.c_str(),
		    sectionTypeInfo[section.type].name.c_str(),
		    where,
		    section.org + section.size,
		    endaddr(section.type) + 1
		);
	// Otherwise there is overlap with another section
	else
		errx(
		    "Unable to place \"%s\" (%s section) %s: section overlaps with \"%s\"",
		    section.name.c_str(),
		    sectionTypeInfo[section.type].name.c_str(),
		    where,
		    out_OverlappingSection(section)->name.c_str()
		);
}

#define BANK_CONSTRAINED  (1 << 2)
#define ORG_CONSTRAINED   (1 << 1)
#define ALIGN_CONSTRAINED (1 << 0)
static std::deque<Section *> unassignedSections[1 << 3];

/*
 * Categorize a section depending on how constrained it is
 * This is so the most-constrained sections are placed first
 * @param section The section to categorize
 */
static void categorizeSection(Section &section) {
	uint8_t constraints = 0;

	if (section.isBankFixed)
		constraints |= BANK_CONSTRAINED;
	if (section.isAddressFixed)
		constraints |= ORG_CONSTRAINED;
	// Can't have both!
	else if (section.isAlignFixed)
		constraints |= ALIGN_CONSTRAINED;

	std::deque<Section *> &sections = unassignedSections[constraints];
	auto pos = sections.begin();

	// Insert section while keeping the list sorted by decreasing size
	while (pos != sections.end() && (*pos)->size > section.size)
		pos++;
	sections.insert(pos, &section);

	nbSectionsToAssign++;
}

void assign_AssignSections() {
	verbosePrint("Beginning assignment...\n");

	// Initialize assignment

	initFreeSpace();

	// Generate linked lists of sections to assign
	nbSectionsToAssign = 0;
	sect_ForEach(categorizeSection);

	// Place sections, starting with the most constrained

	// Specially process fully-constrained sections because of overlaying
	verbosePrint("Assigning bank+org-constrained...\n");
	for (Section *section : unassignedSections[BANK_CONSTRAINED | ORG_CONSTRAINED])
		placeSection(*section);

	// If all sections were fully constrained, we have nothing left to do
	if (!nbSectionsToAssign)
		return;

	// Overlaying requires only fully-constrained sections
	verbosePrint("Assigning other sections...\n");
	if (overlayFileName) {
		fprintf(stderr, "FATAL: All sections must be fixed when using an overlay file");
		uint8_t nbSections = 0;
		for (int8_t constraints = BANK_CONSTRAINED | ALIGN_CONSTRAINED; constraints >= 0;
		     constraints--) {
			for (Section *section : unassignedSections[constraints]) {
				fprintf(stderr, "%c \"%s\"", nbSections == 0 ? ';' : ',', section->name.c_str());
				nbSections++;
				if (nbSections == 10)
					goto max_out; // Can't `break` out of a nested loop
			}
		}

max_out:
		if (nbSectionsToAssign != nbSections)
			fprintf(stderr, " and %" PRIu64 " more", nbSectionsToAssign - nbSections);
		fprintf(stderr, " %sn't\n", nbSectionsToAssign == 1 ? "is" : "are");
		exit(1);
	}

	// Assign all remaining sections by decreasing constraint order
	for (int8_t constraints = BANK_CONSTRAINED | ALIGN_CONSTRAINED; constraints >= 0;
	     constraints--) {
		for (Section *section : unassignedSections[constraints])
			placeSection(*section);

		if (!nbSectionsToAssign)
			return;
	}

	unreachable_();
}
