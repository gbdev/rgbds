// SPDX-License-Identifier: MIT

#include "link/section.hpp"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "diagnostics.hpp"
#include "helpers.hpp"

#include "link/warning.hpp"

std::vector<std::unique_ptr<Section>> sectionList;
std::unordered_map<std::string, size_t> sectionMap; // Indexes into `sectionList`

void sect_ForEach(void (*callback)(Section &)) {
	for (std::unique_ptr<Section> &ptr : sectionList) {
		callback(*ptr);
	}
}

static void checkAgainstFixedAddress(Section const &target, Section const &other, uint16_t org) {
	if (target.isAddressFixed) {
		if (target.org != org) {
			errorNoDump(
			    "Section \"%s\" is defined with address $%04" PRIx16 " at ",
			    target.name.c_str(),
			    target.org
			);
			target.src->dump(target.lineNo);
			fprintf(stderr, ", but with address $%04" PRIx16 " at ", other.org);
			other.src->dump(other.lineNo);
			putc('\n', stderr);
			exit(1);
		}
	} else if (target.isAlignFixed) {
		if ((org - target.alignOfs) & target.alignMask) {
			errorNoDump(
			    "Section \"%s\" is defined with %d-byte alignment (offset %" PRIu16 ") at ",
			    target.name.c_str(),
			    target.alignMask + 1,
			    target.alignOfs
			);
			target.src->dump(target.lineNo);
			fprintf(stderr, ", but with address $%04" PRIx16 " at ", other.org);
			other.src->dump(other.lineNo);
			putc('\n', stderr);
			exit(1);
		}
	}
}

static bool checkAgainstFixedAlign(Section const &target, Section const &other, int32_t ofs) {
	if (target.isAddressFixed) {
		if ((target.org - ofs) & other.alignMask) {
			errorNoDump(
			    "Section \"%s\" is defined with address $%04" PRIx16 " at ",
			    target.name.c_str(),
			    target.org
			);
			target.src->dump(target.lineNo);
			fprintf(
			    stderr,
			    ", but with %d-byte alignment (offset %" PRIu16 ") at ",
			    other.alignMask + 1,
			    other.alignOfs
			);
			other.src->dump(other.lineNo);
			putc('\n', stderr);
			exit(1);
		}
		return false;
	} else if (target.isAlignFixed
	           && (other.alignMask & target.alignOfs) != (target.alignMask & ofs)) {
		errorNoDump(
		    "Section \"%s\" is defined with %d-byte alignment (offset %" PRIu16 ") at ",
		    target.name.c_str(),
		    target.alignMask + 1,
		    target.alignOfs
		);
		target.src->dump(target.lineNo);
		fprintf(
		    stderr,
		    ", but with %d-byte alignment (offset %" PRIu16 ") at ",
		    other.alignMask + 1,
		    other.alignOfs
		);
		other.src->dump(other.lineNo);
		putc('\n', stderr);
		exit(1);
	} else {
		return !target.isAlignFixed || (other.alignMask > target.alignMask);
	}
}

static void checkSectUnionCompat(Section &target, Section &other) {
	if (other.isAddressFixed) {
		checkAgainstFixedAddress(target, other, other.org);
		target.isAddressFixed = true;
		target.org = other.org;
	} else if (other.isAlignFixed) {
		if (checkAgainstFixedAlign(target, other, other.alignOfs)) {
			target.isAlignFixed = true;
			target.alignMask = other.alignMask;
		}
	}
}

static void checkFragmentCompat(Section &target, Section &other) {
	if (other.isAddressFixed) {
		uint16_t org = other.org - target.size;
		checkAgainstFixedAddress(target, other, org);
		target.isAddressFixed = true;
		target.org = org;
	} else if (other.isAlignFixed) {
		int32_t ofs = (other.alignOfs - target.size) % (other.alignMask + 1);
		if (ofs < 0) {
			ofs += other.alignMask + 1;
		}
		if (checkAgainstFixedAlign(target, other, ofs)) {
			target.isAlignFixed = true;
			target.alignMask = other.alignMask;
			target.alignOfs = ofs;
		}
	}
}

static void mergeSections(Section &target, std::unique_ptr<Section> &&other) {
	if (target.modifier != other->modifier) {
		errorNoDump(
		    "Section \"%s\" is defined as SECTION %s at ",
		    target.name.c_str(),
		    sectionModNames[target.modifier]
		);
		target.src->dump(target.lineNo);
		fprintf(stderr, ", but as SECTION %s at ", sectionModNames[other->modifier]);
		other->src->dump(other->lineNo);
		putc('\n', stderr);
		exit(1);
	} else if (other->modifier == SECTION_NORMAL) {
		errorNoDump("Section \"%s\" is defined at ", target.name.c_str());
		target.src->dump(target.lineNo);
		fputs(", but also at ", stderr);
		other->src->dump(other->lineNo);
		putc('\n', stderr);
		exit(1);
	} else if (target.type != other->type) {
		errorNoDump(
		    "Section \"%s\" is defined with type %s at ",
		    target.name.c_str(),
		    sectionTypeInfo[target.type].name.c_str()
		);
		target.src->dump(target.lineNo);
		fprintf(stderr, ", but with type %s at ", sectionTypeInfo[other->type].name.c_str());
		other->src->dump(other->lineNo);
		putc('\n', stderr);
		exit(1);
	}

	if (other->isBankFixed) {
		if (!target.isBankFixed) {
			target.isBankFixed = true;
			target.bank = other->bank;
		} else if (target.bank != other->bank) {
			errorNoDump(
			    "Section \"%s\" is defined with bank %" PRIu32 " at ",
			    target.name.c_str(),
			    target.bank
			);
			target.src->dump(target.lineNo);
			fprintf(stderr, ", but with bank %" PRIu32 " at ", other->bank);
			other->src->dump(other->lineNo);
			putc('\n', stderr);
			exit(1);
		}
	}

	switch (other->modifier) {
	case SECTION_UNION:
		checkSectUnionCompat(target, *other);
		if (other->size > target.size) {
			target.size = other->size;
		}
		break;

	case SECTION_FRAGMENT:
		checkFragmentCompat(target, *other);
		// Append `other` to `target`
		other->offset = target.size;
		target.size += other->size;
		// Normally we'd check that `sect_HasData`, but SDCC areas may be `_INVALID` here
		if (!other->data.empty()) {
			target.data.insert(target.data.end(), RANGE(other->data));
			// Adjust patches' PC offsets
			for (Patch &patch : other->patches) {
				patch.pcOffset += other->offset;
			}
		} else if (!target.data.empty()) {
			assume(other->size == 0);
		}
		break;

	case SECTION_NORMAL:
		// LCOV_EXCL_START
		unreachable_();
	}
	// LCOV_EXCL_STOP

	// Note that the order in which fragments are stored in the `nextu` list does not
	// really matter, only that offsets were properly computed above
	other->nextu = std::move(target.nextu);
	target.nextu = std::move(other);
}

void sect_AddSection(std::unique_ptr<Section> &&section) {
	// Check if the section already exists
	if (Section *target = sect_GetSection(section->name); target) {
		mergeSections(*target, std::move(section));
	} else if (section->modifier == SECTION_UNION && sect_HasData(section->type)) {
		fatal(
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
		error(
		    "Section \"%s\" has not been assigned a type by a linker script", section.name.c_str()
		);
		return;
	}

	if (options.is32kMode && section.type == SECTTYPE_ROMX) {
		if (section.isBankFixed && section.bank != 1) {
			error(
			    "Section \"%s\" has type ROMX, which must be in bank 1 (if any) with option `-t`",
			    section.name.c_str()
			);
		} else {
			section.type = SECTTYPE_ROM0;
		}
	}
	if (options.isWRAM0Mode && section.type == SECTTYPE_WRAMX) {
		if (section.isBankFixed && section.bank != 1) {
			error(
			    "Section \"%s\" has type WRAMX, which must be in bank 1 with options `-w` or `-d`",
			    section.name.c_str()
			);
		} else {
			section.type = SECTTYPE_WRAM0;
		}
	}
	if (options.isDmgMode && section.type == SECTTYPE_VRAM && section.bank == 1) {
		error(
		    "Section \"%s\" has type VRAM, which must be in bank 0 with option `-d`",
		    section.name.c_str()
		);
	}

	// Check if alignment is reasonable, this is important to avoid UB
	// An alignment of zero is equivalent to no alignment, basically
	if (section.isAlignFixed && section.alignMask == 0) {
		section.isAlignFixed = false;
	}

	// Too large an alignment may not be satisfiable
	if (section.isAlignFixed && (section.alignMask & sectionTypeInfo[section.type].startAddr)) {
		error(
		    "Section \"%s\" has type %s, which cannot be aligned to $%04x bytes",
		    section.name.c_str(),
		    sectionTypeInfo[section.type].name.c_str(),
		    section.alignMask + 1
		);
	}

	uint32_t minbank = sectionTypeInfo[section.type].firstBank,
	         maxbank = sectionTypeInfo[section.type].lastBank;

	if (section.isBankFixed && section.bank < minbank && section.bank > maxbank) {
		error(
		    minbank == maxbank
		        ? "Cannot place section \"%s\" in bank %" PRIu32 ", it must be %" PRIu32
		        : "Cannot place section \"%s\" in bank %" PRIu32 ", it must be between %" PRIu32
		          " and %" PRIu32,
		    section.name.c_str(),
		    section.bank,
		    minbank,
		    maxbank
		);
	}

	// Check if section has a chance to be placed
	if (section.size > sectionTypeInfo[section.type].size) {
		error(
		    "Section \"%s\" is bigger than the max size for that type: $%" PRIx16 " > $%" PRIx16,
		    section.name.c_str(),
		    section.size,
		    sectionTypeInfo[section.type].size
		);
	}

	// Translate loose constraints to strong ones when they're equivalent

	if (minbank == maxbank) {
		section.bank = minbank;
		section.isBankFixed = true;
	}

	if (section.isAddressFixed) {
		// It doesn't make sense to have both org and alignment set
		if (section.isAlignFixed) {
			if ((section.org & section.alignMask) != section.alignOfs) {
				error(
				    "Section \"%s\"'s fixed address doesn't match its alignment",
				    section.name.c_str()
				);
			}
			section.isAlignFixed = false;
		}

		// Ensure the target address is valid
		if (section.org < sectionTypeInfo[section.type].startAddr
		    || section.org > endaddr(section.type)) {
			error(
			    "Section \"%s\"'s fixed address $%04" PRIx16 " is outside of range [$%04" PRIx16
			    "; $%04" PRIx16 "]",
			    section.name.c_str(),
			    section.org,
			    sectionTypeInfo[section.type].startAddr,
			    endaddr(section.type)
			);
		}

		if (section.org + section.size > endaddr(section.type) + 1) {
			error(
			    "Section \"%s\"'s end address $%04x is greater than last address $%04x",
			    section.name.c_str(),
			    section.org + section.size,
			    endaddr(section.type) + 1
			);
		}
	}
}

void sect_DoSanityChecks() {
	sect_ForEach(doSanityChecks);
}
