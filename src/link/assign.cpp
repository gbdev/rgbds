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

struct FreeSpace {
	uint16_t address;
	uint16_t size;
};

// Table of free space for each bank
static std::vector<std::deque<FreeSpace>> memory[SECTTYPE_INVALID];

struct Scrambling {
	uint16_t romxOfs = 0;
	uint16_t sramOfs = 0;
	uint16_t wramxOfs = 0;

	// Helper for the next function, to give names to its two returned values.
	struct ScramblingInfo {
		uint16_t &curOfs;
		uint16_t maxOfs;
	};
	std::optional<ScramblingInfo> getInfoFor(SectionType type) {
		switch (type) {
		case SECTTYPE_ROMX:
			return {
			    {romxOfs, options.scrambleROMX}
			};
		case SECTTYPE_SRAM:
			return {
			    {sramOfs, options.scrambleSRAM}
			};
		case SECTTYPE_WRAMX:
			return {
			    {wramxOfs, options.scrambleWRAMX}
			};

		// Non-banked sections don't need scrambling support...
		case SECTTYPE_ROM0:
		case SECTTYPE_WRAM0:
		case SECTTYPE_OAM:
		case SECTTYPE_HRAM:
			assume(!sectionTypeInfo[type].isBanked());
			return std::nullopt;
		// ...but VRAM doesn't either, regardless of whether it's banked or not.
		case SECTTYPE_VRAM:
			return std::nullopt;

		case SECTTYPE_INVALID:
			unreachable_();
		}
		return std::nullopt; // Dead code, but some compilers don't recognize that.
	}
};
static Scrambling scrambling;

struct MemoryLocation {
	uint16_t address;
	uint32_t bank;

	static MemoryLocation initFor(Section const &section) {
		MemoryLocation location;

		if (section.isAddressFixed) { // This will never change.
			location.address = section.org;
		}

		if (section.isBankFixed) {
			location.bank = section.bank;
		} else {
			location.bank = section.typeInfo().firstBank;

			if (auto info = scrambling.getInfoFor(section.type);
			    info.has_value() && info->maxOfs != 0) { // If scrambling is enabled...
				// ...then we will begin the search from a different offset for each section.
				// Go to the next offset (backwards), wrapping around. (Thus, no underflow!)
				info->curOfs = (info->curOfs != 0 ? info->curOfs : info->maxOfs) - 1;
				location.bank += info->curOfs;
			}
		}

		return location;
	}

	// Try again in the next bank, if one is available.
	[[nodiscard("This returns whether iteration can be continued")]]
	bool goToNextApplicableBankFor(Section const &section) {
		assume(bank >= section.typeInfo().firstBank);
		assume(bank <= section.typeInfo().lastBank);

		if (section.isBankFixed) {
			// We have already tried the only possible bank.
			return false;
		}

		// Try scrambled banks in descending order until no bank in the scrambled range is
		// available.
		if (auto info = scrambling.getInfoFor(section.type);
		    info.has_value() && info->maxOfs != 0) {
			// All floating sections within a scrambled region should be
			// within the scrambled bank pool.
			assume(bank < info->maxOfs + section.typeInfo().firstBank);

			uint16_t ofsWithinPool = bank - section.typeInfo().firstBank;
			// Go to the next bank (backwards), wrapping around. (Thus, no overflow!)
			ofsWithinPool = (ofsWithinPool != 0 ? ofsWithinPool : info->maxOfs) - 1;

			bank = ofsWithinPool + section.typeInfo().firstBank;
			// Keep iterating unless we have wrapped back around to the start offset.
			return ofsWithinPool != info->curOfs;
		}

		// Otherwise, try in ascending order.
		if (bank == section.typeInfo().lastBank) {
			return false;
		}
		++bank;
		return true;
	}
};

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

// Returns a suitable free space index into `memory[section->type]` at which to place the given
// section, or `std::nullopt` if none was found.
static std::optional<size_t> getPlacement(Section const &section, MemoryLocation &location) {
	SectionTypeInfo const &typeInfo = section.typeInfo();

	do {
		assume(location.bank >= section.typeInfo().firstBank);
		assume(location.bank <= section.typeInfo().lastBank);

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
				// If the alignment is fixed, move to the next aligned location.
				// We have previously ensured alignment to 15 or fewer bits.
				assume(section.alignMask < (1 << 16) - 1);
				uint16_t prevAddress = location.address;
				// Move back to the alignment boundary.
				// Subtracting the alignment offset may underflow on the first check from address
				// $0000, so applying the alignment mask ensures we have a valid address.
				location.address -= section.alignOfs;
				location.address &= ~section.alignMask;
				// Go to the next align boundary and add the alignment offset.
				location.address += section.alignMask + 1 + section.alignOfs;
				// If the aligned address wrapped around past the end of the address space,
				// no further aligned location can fit in this bank.
				if (location.address <= prevAddress) {
					break;
				}
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

		// Try again in the next iteration.
	} while (location.goToNextApplicableBankFor(section));
	return std::nullopt;
}

static std::string getSectionDescription(Section const &section) {
	std::string description = "\"" + section.name + "\" (" + section.typeInfo().name + " section) ";
	if (section.isBankFixed && section.typeInfo().isBanked()) {
		char bank[9];
		snprintf(bank, sizeof(bank), "%02" PRIx32, section.bank);
		if (section.isAddressFixed) {
			char addr[5];
			snprintf(addr, sizeof(addr), "%04" PRIx16, section.org);
			description = description + "at $" + bank + ":" + addr;
		} else if (section.isAlignFixed) {
			char mask[5];
			snprintf(mask, sizeof(mask), "%" PRIx16, static_cast<uint16_t>(~section.alignMask));
			description = description + "in bank $" + bank + " with align mask $" + mask;
		} else {
			description = description + "in bank $" + bank;
		}
	} else {
		if (section.isAddressFixed) {
			char addr[5];
			snprintf(addr, sizeof(addr), "%04" PRIx16, section.org);
			description = description + "at address $" + addr;
		} else if (section.isAlignFixed) {
			char mask[5], offset[5];
			snprintf(mask, sizeof(mask), "%" PRIx16, static_cast<uint16_t>(~section.alignMask));
			snprintf(offset, sizeof(offset), "%" PRIx16, section.alignOfs);
			description = description + "with align mask $" + mask + " and offset $" + offset;
		} else {
			description = description + "anywhere";
		}

		if (auto info = scrambling.getInfoFor(section.type);
		    info.has_value() && info->maxOfs != 0) { // Only mention scrambling if it is enabled.
			char size[6];
			snprintf(size, sizeof(size), "%" PRIu16, info->maxOfs);
			description = description + " within the " + size + " scrambled banks";
		}
	}

	return description;
}

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

// Places a section in a suitable location, or error out if it fails to.
// Due to the implemented algorithm, this should be called with sections of decreasing size!
static void placeSection(Section &section) {
	MemoryLocation location = MemoryLocation::initFor(section);

	// Specially handle 0-byte SECTIONs, as they can't overlap anything
	if (section.size == 0) {
		// Unless the SECTION has a fixed address or non-zero alignment offset, the starting
		// address is fine for any alignment, as checked in `sect_DoSanityChecks`.
		location.address = section.isAddressFixed ? section.org : section.typeInfo().startAddr;
		if (section.isAlignFixed && !section.isAddressFixed) {
			if (uint16_t offset = (location.address - section.alignOfs) & section.alignMask;
			    offset != 0) {
				location.address += section.alignMask + 1 - offset;
			}
		}
		assignSection(section, location);
		return;
	}

	// Place section using first-fit decreasing algorithm
	// https://en.wikipedia.org/wiki/Bin_packing_problem#First-fit_algorithm
	if (std::optional<size_t> spaceIdx = getPlacement(section, location); spaceIdx) {
		std::deque<FreeSpace> &bankMem =
		    memory[section.type][location.bank - section.typeInfo().firstBank];
		FreeSpace &freeSpace = bankMem[*spaceIdx];

		assignSection(section, location);

		// Update the free space
		assume(section.org + section.size <= UINT16_MAX);
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
	} else if (uint16_t onePastEnd = section.typeInfo().endAddr() + 1;
	           section.org + section.size > onePastEnd) {
		// If the section just can't fit the bank, report that
		fatal(
		    "Unable to place %s: section runs past end of region ($%04x > $%04x)",
		    getSectionDescription(section).c_str(),
		    section.org + section.size,
		    onePastEnd
		);
	} else {
		// Otherwise there is overlap with another section
		Section const *overlap = out_OverlappingSection(section);
		assume(overlap != nullptr);
		fatal(
		    "Unable to place %s: section overlaps with \"%s\"",
		    getSectionDescription(section).c_str(),
		    overlap->name.c_str()
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
		SectionTypeInfo const &typeInfo = sectionTypeInfo[type];
		memory[type].resize(typeInfo.nbBanks());
		for (std::deque<FreeSpace> &bankMem : memory[type]) {
			bankMem.push_back({
			    .address = typeInfo.startAddr,
			    .size = typeInfo.size,
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
