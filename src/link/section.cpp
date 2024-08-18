/* SPDX-License-Identifier: MIT */

#include "link/section.hpp"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "error.hpp"
#include "helpers.hpp"

std::vector<std::unique_ptr<Section>> sectionList;
std::unordered_map<std::string, size_t> sectionMap; // Indexes into `sectionList`

void sect_ForEach(void (*callback)(Section &)) {
	for (auto it = sectionList.rbegin(); it != sectionList.rend(); it++)
		callback(*it->get());
}

static void checkSectUnionCompat(Section &target, Section &other) {
	if (other.isAddressFixed) {
		if (target.isAddressFixed) {
			if (target.org != other.org)
				errx(
				    "Section \"%s\" is defined with conflicting addresses $%04" PRIx16
				    " and $%04" PRIx16,
				    other.name.c_str(),
				    target.org,
				    other.org
				);
		} else if (target.isAlignFixed) {
			if ((other.org - target.alignOfs) & target.alignMask)
				errx(
				    "Section \"%s\" is defined with conflicting %d-byte alignment (offset %" PRIu16
				    ") and address $%04" PRIx16,
				    other.name.c_str(),
				    target.alignMask + 1,
				    target.alignOfs,
				    other.org
				);
		}
		target.isAddressFixed = true;
		target.org = other.org;

	} else if (other.isAlignFixed) {
		if (target.isAddressFixed) {
			if ((target.org - other.alignOfs) & other.alignMask)
				errx(
				    "Section \"%s\" is defined with conflicting address $%04" PRIx16
				    " and %d-byte alignment (offset %" PRIu16 ")",
				    other.name.c_str(),
				    target.org,
				    other.alignMask + 1,
				    other.alignOfs
				);
		} else if (target.isAlignFixed
		           && (other.alignMask & target.alignOfs) != (target.alignMask & other.alignOfs)) {
			errx(
			    "Section \"%s\" is defined with conflicting %d-byte alignment (offset %" PRIu16
			    ") and %d-byte alignment (offset %" PRIu16 ")",
			    other.name.c_str(),
			    target.alignMask + 1,
			    target.alignOfs,
			    other.alignMask + 1,
			    other.alignOfs
			);
		} else if (!target.isAlignFixed || (other.alignMask > target.alignMask)) {
			target.isAlignFixed = true;
			target.alignMask = other.alignMask;
		}
	}
}

static void checkFragmentCompat(Section &target, Section &other) {
	if (other.isAddressFixed) {
		uint16_t org = other.org - target.size;

		if (target.isAddressFixed) {
			if (target.org != org)
				errx(
				    "Section \"%s\" is defined with conflicting addresses $%04" PRIx16
				    " and $%04" PRIx16,
				    other.name.c_str(),
				    target.org,
				    other.org
				);

		} else if (target.isAlignFixed) {
			if ((org - target.alignOfs) & target.alignMask)
				errx(
				    "Section \"%s\" is defined with conflicting %d-byte alignment (offset %" PRIu16
				    ") and address $%04" PRIx16,
				    other.name.c_str(),
				    target.alignMask + 1,
				    target.alignOfs,
				    other.org
				);
		}
		target.isAddressFixed = true;
		target.org = org;

	} else if (other.isAlignFixed) {
		int32_t ofs = (other.alignOfs - target.size) % (other.alignMask + 1);

		if (ofs < 0)
			ofs += other.alignMask + 1;

		if (target.isAddressFixed) {
			if ((target.org - ofs) & other.alignMask)
				errx(
				    "Section \"%s\" is defined with conflicting address $%04" PRIx16
				    " and %d-byte alignment (offset %" PRIu16 ")",
				    other.name.c_str(),
				    target.org,
				    other.alignMask + 1,
				    other.alignOfs
				);

		} else if (target.isAlignFixed && (other.alignMask & target.alignOfs) != (target.alignMask & ofs)) {
			errx(
			    "Section \"%s\" is defined with conflicting %d-byte alignment (offset %" PRIu16
			    ") and %d-byte alignment (offset %" PRIu16 ")",
			    other.name.c_str(),
			    target.alignMask + 1,
			    target.alignOfs,
			    other.alignMask + 1,
			    other.alignOfs
			);

		} else if (!target.isAlignFixed || (other.alignMask > target.alignMask)) {
			target.isAlignFixed = true;
			target.alignMask = other.alignMask;
			target.alignOfs = ofs;
		}
	}
}

static void mergeSections(Section &target, std::unique_ptr<Section> &&other, SectionModifier mod) {
	// Common checks

	if (target.type != other->type)
		errx(
		    "Section \"%s\" is defined with conflicting types %s and %s",
		    other->name.c_str(),
		    sectionTypeInfo[target.type].name.c_str(),
		    sectionTypeInfo[other->type].name.c_str()
		);

	if (other->isBankFixed) {
		if (!target.isBankFixed) {
			target.isBankFixed = true;
			target.bank = other->bank;
		} else if (target.bank != other->bank) {
			errx(
			    "Section \"%s\" is defined with conflicting banks %" PRIu32 " and %" PRIu32,
			    other->name.c_str(),
			    target.bank,
			    other->bank
			);
		}
	}

	switch (mod) {
	case SECTION_UNION:
		checkSectUnionCompat(target, *other);
		if (other->size > target.size)
			target.size = other->size;
		break;

	case SECTION_FRAGMENT:
		checkFragmentCompat(target, *other);
		// Append `other` to `target`
		// Note that the order in which fragments are stored in the `nextu` list does not
		// really matter, only that offsets are properly computed
		other->offset = target.size;
		target.size += other->size;
		// Normally we'd check that `sect_HasData`, but SDCC areas may be `_INVALID` here
		if (!other->data.empty()) {
			target.data.insert(target.data.end(), RANGE(other->data));
			// Adjust patches' PC offsets
			for (Patch &patch : other->patches)
				patch.pcOffset += other->offset;
		} else if (!target.data.empty()) {
			assume(other->size == 0);
		}
		break;

	case SECTION_NORMAL:
		unreachable_();
	}

	other->nextu = std::move(target.nextu);
	target.nextu = std::move(other);
}

void sect_AddSection(std::unique_ptr<Section> &&section) {
	// Check if the section already exists
	if (Section *other = sect_GetSection(section->name); other) {
		if (section->modifier != other->modifier)
			errx(
			    "Section \"%s\" defined as %s and %s",
			    section->name.c_str(),
			    sectionModNames[section->modifier],
			    sectionModNames[other->modifier]
			);
		else if (section->modifier == SECTION_NORMAL)
			errx("Section name \"%s\" is already in use", section->name.c_str());
		else
			mergeSections(*other, std::move(section), section->modifier);
	} else if (section->modifier == SECTION_UNION && sect_HasData(section->type)) {
		errx(
		    "Section \"%s\" is of type %s, which cannot be unionized",
		    section->name.c_str(),
		    sectionTypeInfo[section->type].name.c_str()
		);
	} else {
		// If not, add it
		sectionMap.emplace(section->name, sectionList.size());
		sectionList.push_back(std::move(section));
	}
}

Section *sect_GetSection(std::string const &name) {
	auto search = sectionMap.find(name);
	return search != sectionMap.end() ? sectionList[search->second].get() : nullptr;
}

static void doSanityChecks(Section &section) {
	// Sanity check the section's type
	if (section.type < 0 || section.type >= SECTTYPE_INVALID) {
		// This is trapped early in RGBDS objects (because then the format is not parseable),
		// which leaves SDAS objects.
		error(nullptr, 0, "Section \"%s\" has not been assigned a type by a linker script", section.name.c_str());
		return;
	}

	if (is32kMode && section.type == SECTTYPE_ROMX) {
		if (section.isBankFixed && section.bank != 1)
			error(
			    nullptr,
			    0,
			    "%s: ROMX sections must be in bank 1 (if any) with option -t",
			    section.name.c_str()
			);
		else
			section.type = SECTTYPE_ROM0;
	}
	if (isWRAM0Mode && section.type == SECTTYPE_WRAMX) {
		if (section.isBankFixed && section.bank != 1)
			error(
			    nullptr,
			    0,
			    "%s: WRAMX sections must be in bank 1 with options -w or -d",
			    section.name.c_str()
			);
		else
			section.type = SECTTYPE_WRAM0;
	}
	if (isDmgMode && section.type == SECTTYPE_VRAM && section.bank == 1)
		error(nullptr, 0, "%s: VRAM bank 1 can't be used with option -d", section.name.c_str());

	// Check if alignment is reasonable, this is important to avoid UB
	// An alignment of zero is equivalent to no alignment, basically
	if (section.isAlignFixed && section.alignMask == 0)
		section.isAlignFixed = false;

	// Too large an alignment may not be satisfiable
	if (section.isAlignFixed && (section.alignMask & sectionTypeInfo[section.type].startAddr))
		error(
		    nullptr,
		    0,
		    "%s: %s sections cannot be aligned to $%04x bytes",
		    section.name.c_str(),
		    sectionTypeInfo[section.type].name.c_str(),
		    section.alignMask + 1
		);

	uint32_t minbank = sectionTypeInfo[section.type].firstBank,
	         maxbank = sectionTypeInfo[section.type].lastBank;

	if (section.isBankFixed && section.bank < minbank && section.bank > maxbank)
		error(
		    nullptr,
		    0,
		    minbank == maxbank
		        ? "Cannot place section \"%s\" in bank %" PRIu32 ", it must be %" PRIu32
		        : "Cannot place section \"%s\" in bank %" PRIu32 ", it must be between %" PRIu32
		          " and %" PRIu32,
		    section.name.c_str(),
		    section.bank,
		    minbank,
		    maxbank
		);

	// Check if section has a chance to be placed
	if (section.size > sectionTypeInfo[section.type].size)
		error(
		    nullptr,
		    0,
		    "Section \"%s\" is bigger than the max size for that type: $%" PRIx16 " > $%" PRIx16,
		    section.name.c_str(),
		    section.size,
		    sectionTypeInfo[section.type].size
		);

	// Translate loose constraints to strong ones when they're equivalent

	if (minbank == maxbank) {
		section.bank = minbank;
		section.isBankFixed = true;
	}

	if (section.isAddressFixed) {
		// It doesn't make sense to have both org and alignment set
		if (section.isAlignFixed) {
			if ((section.org & section.alignMask) != section.alignOfs)
				error(
				    nullptr,
				    0,
				    "Section \"%s\"'s fixed address doesn't match its alignment",
				    section.name.c_str()
				);
			section.isAlignFixed = false;
		}

		// Ensure the target address is valid
		if (section.org < sectionTypeInfo[section.type].startAddr
		    || section.org > endaddr(section.type))
			error(
			    nullptr,
			    0,
			    "Section \"%s\"'s fixed address $%04" PRIx16 " is outside of range [$%04" PRIx16
			    "; $%04" PRIx16 "]",
			    section.name.c_str(),
			    section.org,
			    sectionTypeInfo[section.type].startAddr,
			    endaddr(section.type)
			);

		if (section.org + section.size > endaddr(section.type) + 1)
			error(
			    nullptr,
			    0,
			    "Section \"%s\"'s end address $%04x is greater than last address $%04x",
			    section.name.c_str(),
			    section.org + section.size,
			    endaddr(section.type) + 1
			);
	}
}

void sect_DoSanityChecks() {
	sect_ForEach(doSanityChecks);
}
