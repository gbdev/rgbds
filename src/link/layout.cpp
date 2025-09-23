// SPDX-License-Identifier: MIT

#include "link/layout.hpp"

#include <array>
#include <bit>
#include <inttypes.h>
#include <stdint.h>
#include <vector>

#include "helpers.hpp"
#include "linkdefs.hpp"

#include "link/section.hpp"
#include "link/warning.hpp"

static std::array<std::vector<uint16_t>, SECTTYPE_INVALID> curAddr;
static SectionType activeType = SECTTYPE_INVALID; // Index into curAddr
static uint32_t activeBankIdx;                    // Index into curAddr[activeType]
static bool isPcFloating;
static uint16_t floatingAlignMask;
static uint16_t floatingAlignOffset;

static void setActiveTypeAndIdx(SectionType type, uint32_t idx) {
	activeType = type;
	activeBankIdx = idx;
	isPcFloating = false;
	if (curAddr[activeType].size() <= activeBankIdx) {
		curAddr[activeType].resize(activeBankIdx + 1, sectionTypeInfo[type].startAddr);
	}
}

void layout_SetFloatingSectionType(SectionType type) {
	if (sectTypeBanks(type) == 1) {
		// There is only a single bank anyway, so just set the index to 0.
		setActiveTypeAndIdx(type, 0);
	} else {
		activeType = type;
		activeBankIdx = UINT32_MAX;
		// Force PC to be floating for this kind of section.
		// Because we wouldn't know how to index into `curAddr[activeType]`!
		isPcFloating = true;
		floatingAlignMask = 0;
		floatingAlignOffset = 0;
	}
}

void layout_SetSectionType(SectionType type) {
	if (sectTypeBanks(type) != 1) {
		scriptError("A bank number must be specified for %s", sectionTypeInfo[type].name.c_str());
		// Keep going with a default value for the bank index.
	}

	setActiveTypeAndIdx(type, 0); // There is only a single bank anyway, so just set the index to 0.
}

void layout_SetSectionType(SectionType type, uint32_t bank) {
	SectionTypeInfo const &typeInfo = sectionTypeInfo[type];

	if (bank < typeInfo.firstBank) {
		scriptError(
		    "%s bank %" PRIu32 " does not exist (the minimum is %" PRIu32 ")",
		    typeInfo.name.c_str(),
		    bank,
		    typeInfo.firstBank
		);
		bank = typeInfo.firstBank;
	} else if (bank > typeInfo.lastBank) {
		scriptError(
		    "%s bank %" PRIu32 " does not exist (the maximum is %" PRIu32 ")",
		    typeInfo.name.c_str(),
		    bank,
		    typeInfo.lastBank
		);
	}

	setActiveTypeAndIdx(type, bank - typeInfo.firstBank);
}

void layout_SetAddr(uint32_t addr) {
	if (activeType == SECTTYPE_INVALID) {
		scriptError("Cannot set the current address: no memory region is active");
		return;
	}
	if (activeBankIdx == UINT32_MAX) {
		scriptError("Cannot set the current address: the bank is floating");
		return;
	}

	uint16_t &pc = curAddr[activeType][activeBankIdx];
	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];

	if (addr < pc) {
		scriptError("Cannot decrease the current address (from $%04x to $%04x)", pc, addr);
	} else if (addr > sectTypeEndAddr(activeType)) { // Allow "one past the end" sections.
		scriptError(
		    "Cannot set the current address to $%04" PRIx32 ": %s ends at $%04" PRIx16,
		    addr,
		    typeInfo.name.c_str(),
		    sectTypeEndAddr(activeType)
		);
		pc = sectTypeEndAddr(activeType);
	} else {
		pc = addr;
	}
	isPcFloating = false;
}

void layout_MakeAddrFloating() {
	if (activeType == SECTTYPE_INVALID) {
		scriptError("Cannot make the current address floating: no memory region is active");
		return;
	}

	isPcFloating = true;
	floatingAlignMask = 0;
	floatingAlignOffset = 0;
}

void layout_AlignTo(uint32_t alignment, uint32_t alignOfs) {
	if (activeType == SECTTYPE_INVALID) {
		scriptError("Cannot align: no memory region is active");
		return;
	}

	if (isPcFloating) {
		if (alignment >= 16) {
			layout_SetAddr(floatingAlignOffset);
		} else {
			uint32_t alignSize = 1u << alignment;
			uint32_t alignMask = alignSize - 1;

			if (alignOfs >= alignSize) {
				scriptError(
				    "Cannot align: The alignment offset (%" PRIu32
				    ") must be less than alignment size (%" PRIu32 ")",
				    alignOfs,
				    alignSize
				);
				return;
			}

			floatingAlignMask = alignMask;
			floatingAlignOffset = alignOfs & alignMask;
		}
		return;
	}

	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];
	uint16_t &pc = curAddr[activeType][activeBankIdx];

	if (alignment > 16) {
		scriptError("Cannot align: The alignment (%" PRIu32 ") must be less than 16", alignment);
		return;
	}

	// Let it wrap around, this'll trip the final check if alignment == 16.
	uint16_t length = alignOfs - pc;

	if (alignment < 16) {
		uint32_t alignSize = 1u << alignment;
		uint32_t alignMask = alignSize - 1;

		if (alignOfs >= alignSize) {
			scriptError(
			    "Cannot align: The alignment offset (%" PRIu32
			    ") must be less than alignment size (%" PRIu32 ")",
			    alignOfs,
			    alignSize
			);
			return;
		}

		assume(pc >= typeInfo.startAddr);
		length &= alignMask;
	}

	if (uint16_t offset = pc - typeInfo.startAddr; length > typeInfo.size - offset) {
		scriptError(
		    "Cannot align: the next suitable address after $%04" PRIx16 " is $%04" PRIx16
		    ", past $%04" PRIx16,
		    pc,
		    static_cast<uint16_t>(pc + length),
		    static_cast<uint16_t>(sectTypeEndAddr(activeType) + 1)
		);
		return;
	}

	pc += length;
}

void layout_Pad(uint32_t length) {
	if (activeType == SECTTYPE_INVALID) {
		scriptError("Cannot increase the current address: no memory region is active");
		return;
	}

	if (isPcFloating) {
		floatingAlignOffset = (floatingAlignOffset + length) & floatingAlignMask;
		return;
	}

	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];
	uint16_t &pc = curAddr[activeType][activeBankIdx];

	assume(pc >= typeInfo.startAddr);
	if (uint16_t offset = pc - typeInfo.startAddr; length + offset > typeInfo.size) {
		scriptError(
		    "Cannot increase the current address by %u bytes: only %u bytes to $%04" PRIx16,
		    length,
		    typeInfo.size - offset,
		    static_cast<uint16_t>(sectTypeEndAddr(activeType) + 1)
		);
	} else {
		pc += length;
	}
}

void layout_PlaceSection(std::string const &name, bool isOptional) {
	if (activeType == SECTTYPE_INVALID) {
		scriptError("No memory region has been specified to place section \"%s\" in", name.c_str());
		return;
	}

	Section *section = sect_GetSection(name.c_str());
	if (!section) {
		if (!isOptional) {
			scriptError("Undefined section \"%s\"", name.c_str());
		}
		return;
	}

	SectionTypeInfo const &typeInfo = sectionTypeInfo[activeType];
	assume(section->offset == 0);
	// Check that the linker script doesn't contradict what the code says.
	if (section->type == SECTTYPE_INVALID) {
		// A section that has data must get assigned a type that requires data.
		if (!sectTypeHasData(activeType) && !section->data.empty()) {
			scriptError(
			    "\"%s\" is specified to be a %s section, but it contains data",
			    name.c_str(),
			    typeInfo.name.c_str()
			);
		} else if (sectTypeHasData(activeType) && section->data.empty() && section->size != 0) {
			// A section that lacks data can only be assigned to a type that requires data
			// if it's empty.
			scriptError(
			    "\"%s\" is specified to be a %s section, but it does not contain data",
			    name.c_str(),
			    typeInfo.name.c_str()
			);
		} else {
			// SDCC areas don't have a type assigned yet, so the linker script gives them one.
			for (Section *piece = section; piece != nullptr; piece = piece->nextPiece.get()) {
				piece->type = activeType;
			}
		}
	} else if (section->type != activeType) {
		scriptError(
		    "\"%s\" is specified to be a %s section, but it is already a %s section",
		    name.c_str(),
		    typeInfo.name.c_str(),
		    sectionTypeInfo[section->type].name.c_str()
		);
	}

	if (activeBankIdx == UINT32_MAX) {
		section->isBankFixed = false;
	} else {
		uint32_t bank = activeBankIdx + typeInfo.firstBank;
		if (section->isBankFixed && bank != section->bank) {
			scriptError(
			    "The linker script places section \"%s\" in %s bank %" PRIu32
			    ", but it was already defined in bank %" PRIu32,
			    name.c_str(),
			    sectionTypeInfo[section->type].name.c_str(),
			    bank,
			    section->bank
			);
		}
		section->isBankFixed = true;
		section->bank = bank;
	}

	if (!isPcFloating) {
		uint16_t &org = curAddr[activeType][activeBankIdx];
		if (section->isAddressFixed && org != section->org) {
			scriptError(
			    "The linker script assigns section \"%s\" to address $%04" PRIx16
			    ", but it was already at $%04" PRIx16,
			    name.c_str(),
			    org,
			    section->org
			);
		} else if (section->isAlignFixed && (org & section->alignMask) != section->alignOfs) {
			uint8_t alignment = std::countr_one(section->alignMask);
			scriptError(
			    "The linker script assigns section \"%s\" to address $%04" PRIx16
			    ", but that would be ALIGN[%" PRIu8 ", %" PRIu16
			    "] instead of the requested ALIGN[%" PRIu8 ", %" PRIu16 "]",
			    name.c_str(),
			    org,
			    alignment,
			    static_cast<uint16_t>(org & section->alignMask),
			    alignment,
			    section->alignOfs
			);
		}
		section->isAddressFixed = true;
		section->isAlignFixed = false; // This can't be set when the above is.
		section->org = org;

		uint16_t curOfs = org - typeInfo.startAddr;
		if (section->size > typeInfo.size - curOfs) {
			uint16_t overflowSize = section->size - (typeInfo.size - curOfs);
			scriptError(
			    "The linker script assigns section \"%s\" to address $%04" PRIx16
			    ", but then it would overflow %s by %" PRIu16 " byte%s",
			    name.c_str(),
			    org,
			    typeInfo.name.c_str(),
			    overflowSize,
			    overflowSize == 1 ? "" : "s"
			);
			// Fill as much as possible without going out of bounds.
			org = typeInfo.startAddr + typeInfo.size;
		} else {
			org += section->size;
		}
	} else {
		section->isAddressFixed = false;
		section->isAlignFixed = floatingAlignMask != 0;
		section->alignMask = floatingAlignMask;
		section->alignOfs = floatingAlignOffset;

		floatingAlignOffset = (floatingAlignOffset + section->size) & floatingAlignMask;
	}
}
