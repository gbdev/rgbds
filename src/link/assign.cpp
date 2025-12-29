// SPDX-License-Identifier: MIT

#include "link/assign.hpp"

#include <deque>
#include <inttypes.h>
#include <optional>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "helpers.hpp"
#include "itertools.hpp"
#include "linkdefs.hpp"
#include "verbosity.hpp"

#include "link/main.hpp"
#include "link/output.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"
#include "link/warning.hpp"

struct MemoryLocation {
	uint16_t address;
	uint32_t bank;
};

struct FreeSpace {
	uint16_t address;
	uint16_t size;
};

// Table of free space for each bank
static std::vector<std::deque<FreeSpace>> memory[SECTTYPE_INVALID];

// Assigns a section to a given memory location
static void assignSection(Section &section, MemoryLocation const &location) {
	// Propagate the assigned location to all UNIONs/FRAGMENTs
	// so `jr` patches in them will have the correct offset
	for (Section &piece : section.pieces()) {
		piece.org = location.address;
		piece.bank = location.bank;
	}
	out_AddSection(section);
}

// Checks whether a given location is suitable for placing a given section
// This checks not only that the location has enough room for the section, but
// also that the constraints (alignment...) are respected.
static bool isLocationSuitable(
    Section const &section, FreeSpace const &freeSpace, MemoryLocation const &location
) {
	if (section.isAddressFixed && section.org != location.address) {
		return false;
	}

	if (section.isAlignFixed && ((location.address - section.alignOfs) & section.alignMask)) {
		return false;
	}

	if (location.address < freeSpace.address) {
		return false;
	}

	return location.address + section.size <= freeSpace.address + freeSpace.size;
}

static MemoryLocation getStartLocation(Section const &section) {
	static uint16_t curScrambleROM = 0;
	static uint8_t curScrambleWRAM = 0;
	static int8_t curScrambleSRAM = 0;

	MemoryLocation location;

	// Determine which bank we should start searching in
	if (section.isBankFixed) {
		location.bank = section.bank;
	} else if (options.scrambleROMX && section.type == SECTTYPE_ROMX) {
		if (curScrambleROM < 1) {
			curScrambleROM = options.scrambleROMX;
		}
		location.bank = curScrambleROM--;
	} else if (options.scrambleWRAMX && section.type == SECTTYPE_WRAMX) {
		if (curScrambleWRAM < 1) {
			curScrambleWRAM = options.scrambleWRAMX;
		}
		location.bank = curScrambleWRAM--;
	} else if (options.scrambleSRAM && section.type == SECTTYPE_SRAM) {
		if (curScrambleSRAM < 0) {
			curScrambleSRAM = options.scrambleSRAM;
		}
		location.bank = curScrambleSRAM--;
	} else {
		location.bank = sectionTypeInfo[section.type].firstBank;
	}

	return location;
}

// Returns a suitable free space index into `memory[section->type]` at which to place the given
// section, or `std::nullopt` if none was found.
static std::optional<size_t> getPlacement(Section const &section, MemoryLocation &location) {
	SectionTypeInfo const &typeInfo = sectionTypeInfo[section.type];

	for (;;) {
		if (location.bank < typeInfo.firstBank
		    || location.bank >= memory[section.type].size() + typeInfo.firstBank) {
			fatal(
			    "Invalid bank for %s section \"%s\": %" PRIu32,
			    sectionTypeInfo[section.type].name.c_str(),
			    section.name.c_str(),
			    location.bank
			);
		}

		// Switch to the beginning of the next bank
		std::deque<FreeSpace> &bankMem = memory[section.type][location.bank - typeInfo.firstBank];
		size_t spaceIdx = 0;

		if (spaceIdx < bankMem.size()) {
			location.address = bankMem[spaceIdx].address;
		}

		// Process locations in that bank
		while (spaceIdx < bankMem.size()) {
			// If that location is OK, return it
			if (isLocationSuitable(section, bankMem[spaceIdx], location)) {
				return spaceIdx;
			}

			// Go to the next *possible* location
			if (section.isAddressFixed) {
				// If the address is fixed, there can be only one candidate block per bank;
				// if we already reached it, give up and try again in the next bank.
				if (location.address >= section.org) {
					break;
				}
				location.address = section.org;
			} else if (section.isAlignFixed) {
				// Move to next aligned location
				// Move back to alignment boundary
				location.address -= section.alignOfs;
				// Ensure we're there (e.g. on first check)
				location.address &= ~section.alignMask;
				// Go to next align boundary and add offset
				location.address += section.alignMask + 1 + section.alignOfs;
			} else if (++spaceIdx < bankMem.size()) {
				// Any location is fine, so, next free block
				location.address = bankMem[spaceIdx].address;
			}

			// If that location is past the current block's end,
			// go forwards until that is no longer the case.
			while (spaceIdx < bankMem.size()
			       && location.address >= bankMem[spaceIdx].address + bankMem[spaceIdx].size) {
				++spaceIdx;
			}

			// Try again with the new location/free space combo
		}

		// Try again in the next bank, if one is available.
		// Try scrambled banks in descending order until no bank in the scrambled range is
		// available. Otherwise, try in ascending order.
		if (section.isBankFixed) {
			return std::nullopt;
		} else if (options.scrambleROMX && section.type == SECTTYPE_ROMX
		           && location.bank <= options.scrambleROMX) {
			if (location.bank > typeInfo.firstBank) {
				--location.bank;
			} else if (options.scrambleROMX < typeInfo.lastBank) {
				location.bank = options.scrambleROMX + 1;
			} else {
				return std::nullopt;
			}
		} else if (options.scrambleWRAMX && section.type == SECTTYPE_WRAMX
		           && location.bank <= options.scrambleWRAMX) {
			if (location.bank > typeInfo.firstBank) {
				--location.bank;
			} else if (options.scrambleWRAMX < typeInfo.lastBank) {
				location.bank = options.scrambleWRAMX + 1;
			} else {
				return std::nullopt;
			}
		} else if (options.scrambleSRAM && section.type == SECTTYPE_SRAM
		           && location.bank <= options.scrambleSRAM) {
			if (location.bank > typeInfo.firstBank) {
				--location.bank;
			} else if (options.scrambleSRAM < typeInfo.lastBank) {
				location.bank = options.scrambleSRAM + 1;
			} else {
				return std::nullopt;
			}
		} else if (location.bank < typeInfo.lastBank) {
			++location.bank;
		} else {
			return std::nullopt;
		}

		// Try again in the next iteration.
	}
}

static std::string getSectionDescription(Section const &section) {
	std::string description =
	    "\"" + section.name + "\" (" + sectionTypeInfo[section.type].name + " section) ";
	if (section.isBankFixed && sectTypeBanks(section.type) != 1) {
		char bank[8];
		snprintf(bank, sizeof(bank), "%02" PRIx32, section.bank);
		if (section.isAddressFixed) {
			char addr[8];
			snprintf(addr, sizeof(addr), "%04" PRIx16, section.org);
			description = description + "at $" + bank + ":" + addr;
		} else if (section.isAlignFixed) {
			char mask[8];
			snprintf(mask, sizeof(mask), "%" PRIx16, static_cast<uint16_t>(~section.alignMask));
			description = description + "in bank $" + bank + " with align mask $" + mask;
		} else {
			description = description + "in bank $" + bank;
		}
	} else {
		if (section.isAddressFixed) {
			char addr[8];
			snprintf(addr, sizeof(addr), "%04" PRIx16, section.org);
			description = description + "at address $" + addr;
		} else if (section.isAlignFixed) {
			char mask[8], offset[8];
			snprintf(mask, sizeof(mask), "%" PRIx16, static_cast<uint16_t>(~section.alignMask));
			snprintf(offset, sizeof(offset), "%" PRIx16, section.alignOfs);
			description = description + "with align mask $" + mask + " and offset $" + offset;
		} else {
			description = description + "anywhere";
		}
	}
	return description;
}

// Places a section in a suitable location, or error out if it fails to.
// Due to the implemented algorithm, this should be called with sections of decreasing size!
static void placeSection(Section &section) {
	// Specially handle 0-byte SECTIONs, as they can't overlap anything
	if (section.size == 0) {
		// Unless the SECTION's address was fixed, the starting address
		// is fine for any alignment, as checked in sect_DoSanityChecks.
		MemoryLocation location = {
		    .address =
		        section.isAddressFixed ? section.org : sectionTypeInfo[section.type].startAddr,
		    .bank = section.isBankFixed ? section.bank : sectionTypeInfo[section.type].firstBank,
		};
		assignSection(section, location);
		return;
	}

	// Place section using first-fit decreasing algorithm
	// https://en.wikipedia.org/wiki/Bin_packing_problem#First-fit_algorithm
	MemoryLocation location = getStartLocation(section);
	if (std::optional<size_t> spaceIdx = getPlacement(section, location); spaceIdx) {
		std::deque<FreeSpace> &bankMem =
		    memory[section.type][location.bank - sectionTypeInfo[section.type].firstBank];
		FreeSpace &freeSpace = bankMem[*spaceIdx];

		assignSection(section, location);

		// Update the free space
		uint16_t sectionEnd = section.org + section.size;
		bool noLeftSpace = freeSpace.address == section.org;
		bool noRightSpace = freeSpace.address + freeSpace.size == sectionEnd;
		if (noLeftSpace && noRightSpace) {
			// The free space is entirely deleted
			bankMem.erase(bankMem.begin() + *spaceIdx);
		} else if (!noLeftSpace && !noRightSpace) {
			// The free space is split in two
			// Append the new space after the original one
			uint16_t size = static_cast<uint16_t>(freeSpace.address + freeSpace.size - sectionEnd);
			bankMem.insert(bankMem.begin() + *spaceIdx + 1, {.address = sectionEnd, .size = size});
			// **`freeSpace` cannot be reused from this point on, because `bankMem.insert`
			// invalidates all references to itself!**

			// Resize the original space (address is unmodified)
			bankMem[*spaceIdx].size = section.org - bankMem[*spaceIdx].address;
		} else {
			// The amount of free spaces doesn't change: resize!
			freeSpace.size -= section.size;
			if (noLeftSpace) {
				// The free space is moved *and* resized
				freeSpace.address += section.size;
			}
		}
		return;
	}

	if (!section.isBankFixed || !section.isAddressFixed) {
		// If a section failed to go to several places, nothing we can report
		fatal("Unable to place %s", getSectionDescription(section).c_str());
	} else if (section.org + section.size > sectTypeEndAddr(section.type) + 1) {
		// If the section just can't fit the bank, report that
		fatal(
		    "Unable to place %s: section runs past end of region ($%04x > $%04x)",
		    getSectionDescription(section).c_str(),
		    section.org + section.size,
		    sectTypeEndAddr(section.type) + 1
		);
	} else {
		// Otherwise there is overlap with another section
		fatal(
		    "Unable to place %s: section overlaps with \"%s\"",
		    getSectionDescription(section).c_str(),
		    out_OverlappingSection(section)->name.c_str()
		);
	}
}

static std::deque<Section *> unassignedSections[1 << 3];
// clang-format off: vertically align values
static constexpr uint8_t BANK_CONSTRAINED  = 1 << 2;
static constexpr uint8_t ORG_CONSTRAINED   = 1 << 1;
static constexpr uint8_t ALIGN_CONSTRAINED = 1 << 0;
// clang-format on
static char const * const constraintNames[] = {
    "un",
    "align-",
    "org-",
    nullptr, // align+org (impossible)
    "bank-",
    "bank+align-",
    "bank+org-",
    nullptr, // bank+align+org (impossible)
};

// Categorize a section depending on how constrained it is.
// This is so the most-constrained sections are placed first.
static void categorizeSection(Section &section) {
	uint8_t constraints = 0;

	if (section.isBankFixed) {
		constraints |= BANK_CONSTRAINED;
	}
	// Can't have both!
	if (section.isAddressFixed) {
		constraints |= ORG_CONSTRAINED;
	} else if (section.isAlignFixed) {
		constraints |= ALIGN_CONSTRAINED;
	}

	std::deque<Section *> &sections = unassignedSections[constraints];

	// Insert section while keeping the list sorted by decreasing size
	auto pos = sections.begin();
	while (pos != sections.end() && (*pos)->size > section.size) {
		++pos;
	}
	sections.insert(pos, &section);
}

static void checkOverlayCompat() {
	auto isFixed = [](uint8_t constraints) {
		return (constraints & BANK_CONSTRAINED) && (constraints & ORG_CONSTRAINED);
	};

	std::string unfixedList;

	size_t nbUnfixedSections = 0;
	for (uint8_t constraints = std::size(unassignedSections); constraints--;) {
		if (!isFixed(constraints)) {
			nbUnfixedSections += unassignedSections[constraints].size();
		}
	}

	if (nbUnfixedSections == 0) {
		return;
	}

	size_t nbListed = 0;
	for (uint8_t constraints = std::size(unassignedSections); constraints--;) {
		if (isFixed(constraints)) {
			continue;
		}

		for (Section const *section : unassignedSections[constraints]) {
			if (nbListed == 10) {
				unfixedList += "\n- and ";
				unfixedList += std::to_string(nbUnfixedSections - nbListed);
				unfixedList += " more";
				break;
			}
			unfixedList += "\n- \"";
			unfixedList += section->name;
			unfixedList += "\" (";
			if (!(constraints & (BANK_CONSTRAINED | ORG_CONSTRAINED))) {
				unfixedList += "bank and address";
			} else if (!(constraints & BANK_CONSTRAINED)) {
				unfixedList += "bank";
			} else {
				assume(!(constraints & ORG_CONSTRAINED));
				unfixedList += "address";
			}
			unfixedList += " not specified)";
			++nbListed;
		}
	}

	fatal(
	    "All sections must be fixed when using an overlay file; %zu %s not:%s",
	    nbUnfixedSections,
	    nbUnfixedSections == 1 ? "is" : "are",
	    unfixedList.c_str()
	);
}

void assign_AssignSections() {
	verbosePrint(VERB_NOTICE, "Beginning assignment...\n");

	// Initialize the free space-modelling structs
	for (SectionType type : EnumSeq(SECTTYPE_INVALID)) {
		memory[type].resize(sectTypeBanks(type));
		for (std::deque<FreeSpace> &bankMem : memory[type]) {
			bankMem.push_back({
			    .address = sectionTypeInfo[type].startAddr,
			    .size = sectionTypeInfo[type].size,
			});
		}
	}

	// Generate linked lists of sections to assign
	static uint64_t nbSectionsToAssign = 0; // `static` so `sect_ForEach` callback can see it
	sect_ForEach([](Section &section) {
		categorizeSection(section);
		++nbSectionsToAssign;
	});

	// Overlaying requires only fully-constrained sections
	if (options.overlayFileName) {
		checkOverlayCompat();
	}

	// Assign sections in decreasing constraint order
	for (uint8_t constraints = std::size(unassignedSections); constraints--;) {
		if (char const *constraintName = constraintNames[constraints]; constraintName) {
			verbosePrint(VERB_INFO, "Assigning %sconstrained sections...\n", constraintName);
		} else {
			assume(unassignedSections[constraints].empty());
		}

		for (Section *section : unassignedSections[constraints]) {
			placeSection(*section);

			// If all sections were fully constrained, we have nothing left to do
			if (!--nbSectionsToAssign) {
				return;
			}
		}
	}

	assume(nbSectionsToAssign == 0);
}
