// SPDX-License-Identifier: MIT

#include "link/assign.hpp"

#include <algorithm>
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
	uint16_t size; // Never zero.

	uint16_t addrOnePast() const { return address + size; }
};

// Table of free space for each bank
static std::vector<std::deque<FreeSpace>> memory[SECTTYPE_INVALID];
using FreeSpaceIter = std::deque<FreeSpace>::iterator;

static std::deque<FreeSpace> &freeSpaceOfBank(Section const &section, uint32_t bank) {
	assume(bank >= section.typeInfo().firstBank);
	assume(bank <= section.typeInfo().lastBank);
	return memory[section.type][bank - section.typeInfo().firstBank];
}

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

		assume(!section.isBankFixed);

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

	void makeAddressAligned(uint16_t alignMask, uint16_t alignOfs) {
		// By how much the address is past the target offset within the current alignment "page".
		uint16_t offset = (address - alignOfs) & alignMask;
		// Move by one page *minus* that "overshoot" offset.
		// If it's 0, then this would move by a whole page, but `& alignMask` resets it back to 0.
		address += ((alignMask + 1) - offset) & alignMask;
	}
};

static FreeSpaceIter tryPlacingInBank(Section const &section, MemoryLocation &location) {
	std::deque<FreeSpace> &bankMem = freeSpaceOfBank(section, location.bank);

	if (section.isAddressFixed) {
		// There is only one candidate location in this bank: the address at which the section is
		// fixed.
		assume(location.address == section.org);

		FreeSpaceIter iter = std::find_if(RANGE(bankMem), [&location](FreeSpace const &freeSpace) {
			// If they both exactly match, that means the next block will begin past the requested
			// addr, so the function would fail anyway.
			return freeSpace.addrOnePast() >= location.address;
		});
		if (iter != bankMem.end()) {
			// We have the first block ending after the section's address, so all that's left is
			// checking that the address does fall into the block, and then that the section fits.
			if (location.address < iter->address
			    || location.address + section.size > iter->addrOnePast()) {
				return bankMem.end(); // Failed! Better luck next bank?
			}
		}
		return iter;

	} else {
		// There are many possible locations within the bank, so we are going to iterate on free
		// blocks. If it is impossible to fit at the earliest (constraint-satisfying) address, then
		// no other address in the block will do; thus, we make only one attempt per block.
		return std::find_if(RANGE(bankMem), [&location, &section](FreeSpace const &freeSpace) {
			location.address = freeSpace.address;
			if (section.isAlignFixed) {
				location.makeAddressAligned(section.alignMask, section.alignOfs);
				// Did it advance past the block? Or, rarely, overflowed?
				if (location.address >= freeSpace.addrOnePast()
				    || location.address < freeSpace.address) {
					return false;
				}
			}

			// Since `location.address` lies within the block,
			// we only need to check that its end address also does.
			return location.address + section.size <= freeSpace.addrOnePast();
		});
	}
}

// Place section using first-fit decreasing algorithm
// <https://en.wikipedia.org/wiki/Bin_packing_problem#First-fit_algorithm>
// Returns an iterator to within `freeSpaceOfBank(section, location.bank)`
// (guaranteeing that `location.bank` remains valid) pointing at the free block that
// the section can go into (at `location.address`).
// The iterator is an end iterator if and only if no suitable location was found.
// `location` is updated accordingly.
static FreeSpaceIter tryPlacing(Section const &section, MemoryLocation &location) {
	if (section.isBankFixed) {
		assume(location.bank == section.bank);
		return tryPlacingInBank(section, location);
	}

	do {
		if (FreeSpaceIter iter = tryPlacingInBank(section, location);
		    iter != freeSpaceOfBank(section, location.bank).end()) {
			return iter; // Found one!
		}
	} while (location.goToNextApplicableBankFor(section));
	// Return a deque's end iterator to signal failure.
	// The exact deque doesn't matter, but the caller will use `freeSpaceOfBank` also.
	return freeSpaceOfBank(section, location.bank).end();
}

static void
    updateFreeSpace(FreeSpaceIter iter, std::deque<FreeSpace> &bankMem, Section const &section) {
	assume(section.org + section.size <= UINT16_MAX);
	uint16_t sectionEnd = section.org + section.size;
	assume(section.org >= iter->address);
	assume(sectionEnd <= iter->addrOnePast());

	bool noLeftSpace = iter->address == section.org;
	bool noRightSpace = iter->address + iter->size == sectionEnd;
	if (noLeftSpace && noRightSpace) {
		// The free space is entirely deleted
		bankMem.erase(iter);
	} else if (!noLeftSpace && !noRightSpace) {
		// The free space is split in two
		uint16_t size = static_cast<uint16_t>(iter->address + iter->size - sectionEnd);
		// Resize the original space (address is unmodified)
		iter->size = section.org - iter->address;
		// Append the new space after the original one
		bankMem.insert(iter + 1, {.address = sectionEnd, .size = size});
		// `iter` cannot be reused from this point on, because `bankMem.insert`
		// invalidates iterators to itself!
	} else {
		// The amount of free spaces doesn't change: resize!
		iter->size -= section.size;
		if (noLeftSpace) {
			// The free space is moved *and* resized
			iter->address += section.size;
		}
	}
}

// Assigns a section to a given memory location
static void assignSection(Section &section, MemoryLocation const &location) {
	assume(location.address >= section.typeInfo().startAddr);
	// Zero-sized sections can start one past the end of their region.
	assume(location.address <= section.typeInfo().endAddr() + 1);
	// This one is not redundant, it guards against overflow!
	assume(location.address + section.size >= section.typeInfo().startAddr);
	assume(location.address + section.size <= section.typeInfo().endAddr() + 1);

	if (section.isAddressFixed) {
		assume(location.address == section.org);
	} else if (section.isAlignFixed) {
		assume((location.address & section.alignMask) == section.alignOfs);
	}
	if (section.isBankFixed) {
		assume(location.bank == section.bank);
	}

	// Propagate the assigned location to all UNIONs/FRAGMENTs
	// so `jr` patches in them will have the correct offset
	for (Section &piece : section.pieces()) {
		piece.org = location.address;
		piece.bank = location.bank;
	}
	out_AddSection(section);
}

static std::string describeConstraintsOf(Section const &section) {
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

// Places a section in a suitable location, or error out if it fails to.
// Due to the implemented algorithm, this should be called with sections of decreasing size!
static void placeSection(Section &section) {
	MemoryLocation location = MemoryLocation::initFor(section);

	// Specially handle 0-byte SECTIONs, as they ignore free space entirely.
	if (section.size == 0) {
		if (!section.isAddressFixed) {
			location.address = section.typeInfo().startAddr;
			if (section.isAlignFixed) {
				location.makeAddressAligned(section.alignMask, section.alignOfs);
			}
		}

		assignSection(section, location);
		return;
	}

	FreeSpaceIter iter = tryPlacing(section, location);
	if (std::deque<FreeSpace> &bankMem = freeSpaceOfBank(section, location.bank);
	    iter != bankMem.end()) {
		assignSection(section, location);

		updateFreeSpace(iter, bankMem, section);
		return;
	}

	if (!section.isBankFixed || !section.isAddressFixed) {
		// If a section failed to go to several places, nothing we can report
		fatal("Unable to place %s", describeConstraintsOf(section).c_str());
	} else if (uint16_t onePastEnd = section.typeInfo().endAddr() + 1;
	           section.org + section.size > onePastEnd) {
		// If the section just can't fit the bank, report that
		fatal(
		    "Unable to place %s: section runs past end of region ($%04x > $%04x)",
		    describeConstraintsOf(section).c_str(),
		    section.org + section.size,
		    onePastEnd
		);
	} else {
		// Otherwise there is overlap with another section
		Section const *overlap = out_OverlappingSection(section);
		assume(overlap != nullptr);
		fatal(
		    "Unable to place %s: section overlaps with \"%s\"",
		    describeConstraintsOf(section).c_str(),
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
