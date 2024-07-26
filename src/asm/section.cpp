/* SPDX-License-Identifier: MIT */

#include "asm/section.hpp"

#include <algorithm>
#include <errno.h>
#include <inttypes.h>
#include <optional>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "helpers.hpp"

#include "asm/fstack.hpp"
#include "asm/lexer.hpp"
#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/rpn.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

uint8_t fillByte;

struct UnionStackEntry {
	uint32_t start;
	uint32_t size;
};

struct SectionStackEntry {
	Section *section;
	Section *loadSection;
	std::optional<std::string> scope; // Section's symbol scope
	uint32_t offset;
	int32_t loadOffset;
	std::stack<UnionStackEntry> unionStack;
};

std::stack<UnionStackEntry> currentUnionStack;
std::deque<SectionStackEntry> sectionStack;
std::deque<Section> sectionList;
std::unordered_map<std::string, size_t> sectionMap; // Indexes into `sectionList`
uint32_t curOffset; // Offset into the current section (see sect_GetSymbolOffset)
Section *currentSection = nullptr;
static Section *currentLoadSection = nullptr;
std::optional<std::string> currentLoadScope = std::nullopt;
int32_t loadOffset; // Offset into the LOAD section's parent (see sect_GetOutputOffset)

// A quick check to see if we have an initialized section
[[nodiscard]] static bool checksection() {
	if (currentSection)
		return true;

	error("Cannot output data outside of a SECTION\n");
	return false;
}

// A quick check to see if we have an initialized section that can contain
// this much initialized data
[[nodiscard]] static bool checkcodesection() {
	if (!checksection())
		return false;

	if (sect_HasData(currentSection->type))
		return true;

	error(
	    "Section '%s' cannot contain code or data (not ROM0 or ROMX)\n",
	    currentSection->name.c_str()
	);
	return false;
}

[[nodiscard]] static bool checkSectionSize(Section const &sect, uint32_t size) {
	uint32_t maxSize = sectionTypeInfo[sect.type].size;

	// If the new size is reasonable, keep going
	if (size <= maxSize)
		return true;

	error(
	    "Section '%s' grew too big (max size = 0x%" PRIX32 " bytes, reached 0x%" PRIX32 ").\n",
	    sect.name.c_str(),
	    maxSize,
	    size
	);
	return false;
}

// Check if the section has grown too much.
[[nodiscard]] static bool reserveSpace(uint32_t delta_size) {
	// This check is here to trap broken code that generates sections that are too big and to
	// prevent the assembler from generating huge object files or trying to allocate too much
	// memory.
	// A check at the linking stage is still necessary.

	// If the section has already overflowed, skip the check to avoid erroring out ad nauseam
	if (currentSection->size != UINT32_MAX
	    && !checkSectionSize(*currentSection, curOffset + loadOffset + delta_size))
		// Mark the section as overflowed, to avoid repeating the error
		currentSection->size = UINT32_MAX;

	if (currentLoadSection && currentLoadSection->size != UINT32_MAX
	    && !checkSectionSize(*currentLoadSection, curOffset + delta_size))
		currentLoadSection->size = UINT32_MAX;

	return currentSection->size != UINT32_MAX
	       && (!currentLoadSection || currentLoadSection->size != UINT32_MAX);
}

Section *sect_FindSectionByName(std::string const &name) {
	auto search = sectionMap.find(name);
	return search != sectionMap.end() ? &sectionList[search->second] : nullptr;
}

#define mask(align) ((1U << (align)) - 1)
#define sectError(...) \
	do { \
		error(__VA_ARGS__); \
		nbSectErrors++; \
	} while (0)

static unsigned int mergeSectUnion(
    Section &sect, SectionType type, uint32_t org, uint8_t alignment, uint16_t alignOffset
) {
	assume(alignment < 16); // Should be ensured by the caller
	unsigned int nbSectErrors = 0;

	// Unionized sections only need "compatible" constraints, and they end up with the strictest
	// combination of both.
	if (sect_HasData(type))
		sectError("Cannot declare ROM sections as UNION\n");

	if (org != (uint32_t)-1) {
		// If both are fixed, they must be the same
		if (sect.org != (uint32_t)-1 && sect.org != org)
			sectError(
			    "Section already declared as fixed at different address $%04" PRIx32 "\n", sect.org
			);
		else if (sect.align != 0 && (mask(sect.align) & (org - sect.alignOfs)))
			sectError(
			    "Section already declared as aligned to %u bytes (offset %" PRIu16 ")\n",
			    1U << sect.align,
			    sect.alignOfs
			);
		else
			// Otherwise, just override
			sect.org = org;

	} else if (alignment != 0) {
		// Make sure any fixed address given is compatible
		if (sect.org != (uint32_t)-1) {
			if ((sect.org - alignOffset) & mask(alignment))
				sectError(
				    "Section already declared as fixed at incompatible address $%04" PRIx32 "\n",
				    sect.org
				);
			// Check if alignment offsets are compatible
		} else if ((alignOffset & mask(sect.align)) != (sect.alignOfs & mask(alignment))) {
			sectError(
			    "Section already declared with incompatible %u"
			    "-byte alignment (offset %" PRIu16 ")\n",
			    1U << sect.align,
			    sect.alignOfs
			);
		} else if (alignment > sect.align) {
			// If the section is not fixed, its alignment is the largest of both
			sect.align = alignment;
			sect.alignOfs = alignOffset;
		}
	}

	return nbSectErrors;
}

static unsigned int
    mergeFragments(Section &sect, uint32_t org, uint8_t alignment, uint16_t alignOffset) {
	assume(alignment < 16); // Should be ensured by the caller
	unsigned int nbSectErrors = 0;

	// Fragments only need "compatible" constraints, and they end up with the strictest
	// combination of both.
	// The merging is however performed at the *end* of the original section!
	if (org != (uint32_t)-1) {
		uint16_t curOrg = org - sect.size;

		// If both are fixed, they must be the same
		if (sect.org != (uint32_t)-1 && sect.org != curOrg)
			sectError(
			    "Section already declared as fixed at incompatible address $%04" PRIx32 "\n",
			    sect.org
			);
		else if (sect.align != 0 && (mask(sect.align) & (curOrg - sect.alignOfs)))
			sectError(
			    "Section already declared as aligned to %u bytes (offset %" PRIu16 ")\n",
			    1U << sect.align,
			    sect.alignOfs
			);
		else
			// Otherwise, just override
			sect.org = curOrg;

	} else if (alignment != 0) {
		int32_t curOfs = (alignOffset - sect.size) % (1U << alignment);

		if (curOfs < 0)
			curOfs += 1U << alignment;

		// Make sure any fixed address given is compatible
		if (sect.org != (uint32_t)-1) {
			if ((sect.org - curOfs) & mask(alignment))
				sectError(
				    "Section already declared as fixed at incompatible address $%04" PRIx32 "\n",
				    sect.org
				);
			// Check if alignment offsets are compatible
		} else if ((curOfs & mask(sect.align)) != (sect.alignOfs & mask(alignment))) {
			sectError(
			    "Section already declared with incompatible %u"
			    "-byte alignment (offset %" PRIu16 ")\n",
			    1U << sect.align,
			    sect.alignOfs
			);
		} else if (alignment > sect.align) {
			// If the section is not fixed, its alignment is the largest of both
			sect.align = alignment;
			sect.alignOfs = curOfs;
		}
	}

	return nbSectErrors;
}

static void mergeSections(
    Section &sect,
    SectionType type,
    uint32_t org,
    uint32_t bank,
    uint8_t alignment,
    uint16_t alignOffset,
    SectionModifier mod
) {
	unsigned int nbSectErrors = 0;

	if (type != sect.type)
		sectError(
		    "Section already exists but with type %s\n", sectionTypeInfo[sect.type].name.c_str()
		);

	if (sect.modifier != mod) {
		sectError("Section already declared as %s section\n", sectionModNames[sect.modifier]);
	} else {
		switch (mod) {
		case SECTION_UNION:
		case SECTION_FRAGMENT:
			nbSectErrors += mod == SECTION_UNION
			                    ? mergeSectUnion(sect, type, org, alignment, alignOffset)
			                    : mergeFragments(sect, org, alignment, alignOffset);

			// Common checks

			// If the section's bank is unspecified, override it
			if (sect.bank == (uint32_t)-1)
				sect.bank = bank;
			// If both specify a bank, it must be the same one
			else if (bank != (uint32_t)-1 && sect.bank != bank)
				sectError("Section already declared with different bank %" PRIu32 "\n", sect.bank);
			break;

		case SECTION_NORMAL:
			sectError("Section already defined previously at ");
			sect.src->dump(sect.fileLine);
			putc('\n', stderr);
			break;
		}
	}

	if (nbSectErrors)
		fatalerror(
		    "Cannot create section \"%s\" (%u error%s)\n",
		    sect.name.c_str(),
		    nbSectErrors,
		    nbSectErrors == 1 ? "" : "s"
		);
}

#undef sectError

// Create a new section, not yet in the list.
static Section *createSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    uint32_t bank,
    uint8_t alignment,
    uint16_t alignOffset,
    SectionModifier mod
) {
	// Add the new section to the list
	Section &sect = sectionList.emplace_back();
	sectionMap.emplace(name, sectionMap.size());

	sect.name = name;
	sect.type = type;
	sect.modifier = mod;
	sect.src = fstk_GetFileStack();
	sect.fileLine = lexer_GetLineNo();
	sect.size = 0;
	sect.org = org;
	sect.bank = bank;
	sect.align = alignment;
	sect.alignOfs = alignOffset;

	// It is only needed to allocate memory for ROM sections.
	if (sect_HasData(type))
		sect.data.resize(sectionTypeInfo[type].size);

	return &sect;
}

// Find a section by name and type. If it doesn't exist, create it.
static Section *getSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
) {
	uint32_t bank = attrs.bank;
	uint8_t alignment = attrs.alignment;
	uint16_t alignOffset = attrs.alignOfs;

	// First, validate parameters, and normalize them if applicable

	if (bank != (uint32_t)-1) {
		if (type != SECTTYPE_ROMX && type != SECTTYPE_VRAM && type != SECTTYPE_SRAM
		    && type != SECTTYPE_WRAMX)
			error("BANK only allowed for ROMX, WRAMX, SRAM, or VRAM sections\n");
		else if (bank < sectionTypeInfo[type].firstBank || bank > sectionTypeInfo[type].lastBank)
			error(
			    "%s bank value $%04" PRIx32 " out of range ($%04" PRIx32 " to $%04" PRIx32 ")\n",
			    sectionTypeInfo[type].name.c_str(),
			    bank,
			    sectionTypeInfo[type].firstBank,
			    sectionTypeInfo[type].lastBank
			);
	} else if (nbbanks(type) == 1) {
		// If the section type only has a single bank, implicitly force it
		bank = sectionTypeInfo[type].firstBank;
	}

	if (alignOffset >= 1 << alignment) {
		error(
		    "Alignment offset (%" PRIu16 ") must be smaller than alignment size (%u)\n",
		    alignOffset,
		    1U << alignment
		);
		alignOffset = 0;
	}

	if (org != (uint32_t)-1) {
		if (org < sectionTypeInfo[type].startAddr || org > endaddr(type))
			error(
			    "Section \"%s\"'s fixed address $%04" PRIx32 " is outside of range [$%04" PRIx16
			    "; $%04" PRIx16 "]\n",
			    name.c_str(),
			    org,
			    sectionTypeInfo[type].startAddr,
			    endaddr(type)
			);
	}

	if (alignment != 0) {
		if (alignment > 16) {
			error("Alignment must be between 0 and 16, not %u\n", alignment);
			alignment = 16;
		}
		// It doesn't make sense to have both alignment and org set
		uint32_t mask = mask(alignment);

		if (org != (uint32_t)-1) {
			if ((org - alignOffset) & mask)
				error("Section \"%s\"'s fixed address doesn't match its alignment\n", name.c_str());
			alignment = 0; // Ignore it if it's satisfied
		} else if (sectionTypeInfo[type].startAddr & mask) {
			error(
			    "Section \"%s\"'s alignment cannot be attained in %s\n",
			    name.c_str(),
			    sectionTypeInfo[type].name.c_str()
			);
			alignment = 0; // Ignore it if it's unattainable
			org = 0;
		} else if (alignment == 16) {
			// Treat an alignment of 16 as fixing the address.
			alignment = 0;
			org = alignOffset;
			// The address is known to be valid, since the alignment itself is.
		}
	}

	// Check if another section exists with the same name; merge if yes, otherwise create one

	Section *sect = sect_FindSectionByName(name);

	if (sect) {
		mergeSections(*sect, type, org, bank, alignment, alignOffset, mod);
	} else {
		sect = createSection(name, type, org, bank, alignment, alignOffset, mod);
	}

	return sect;
}

// Set the current section
static void changeSection() {
	if (!currentUnionStack.empty())
		fatalerror("Cannot change the section within a UNION\n");

	sym_SetCurrentSymbolScope(std::nullopt);
}

bool Section::isSizeKnown() const {
	// SECTION UNION and SECTION FRAGMENT can still grow
	if (modifier != SECTION_NORMAL)
		return false;

	// The current section (or current load section if within one) is still growing
	if (this == currentSection || this == currentLoadSection)
		return false;

	// Any section on the stack is still growing
	for (SectionStackEntry &entry : sectionStack) {
		if (entry.section && entry.section->name == name)
			return false;
	}

	return true;
}

// Set the current section by name and type
void sect_NewSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
) {
	if (currentLoadSection)
		fatalerror("Cannot change the section within a `LOAD` block\n");

	for (SectionStackEntry &entry : sectionStack) {
		if (entry.section && entry.section->name == name)
			fatalerror("Section '%s' is already on the stack\n", name.c_str());
	}

	Section *sect = getSection(name, type, org, attrs, mod);

	changeSection();
	curOffset = mod == SECTION_UNION ? 0 : sect->size;
	loadOffset = 0; // This is still used when checking for section size overflow!
	currentSection = sect;
}

// Set the current section by name and type
void sect_SetLoadSection(
    std::string const &name,
    SectionType type,
    uint32_t org,
    SectionSpec const &attrs,
    SectionModifier mod
) {
	// Important info: currently, UNION and LOAD cannot interact, since UNION is prohibited in
	// "code" sections, whereas LOAD is restricted to them.
	// Therefore, any interactions are NOT TESTED, so lift either of those restrictions at
	// your own peril! ^^

	if (!checkcodesection())
		return;

	if (currentLoadSection) {
		error("`LOAD` blocks cannot be nested\n");
		return;
	}

	if (sect_HasData(type)) {
		error("`LOAD` blocks cannot create a ROM section\n");
		return;
	}

	if (mod == SECTION_FRAGMENT) {
		error("`LOAD FRAGMENT` is not allowed\n");
		return;
	}

	Section *sect = getSection(name, type, org, attrs, mod);

	currentLoadScope = sym_GetCurrentSymbolScope();
	changeSection();
	loadOffset = curOffset - (mod == SECTION_UNION ? 0 : sect->size);
	curOffset -= loadOffset;
	currentLoadSection = sect;
}

void sect_EndLoadSection() {
	if (!currentLoadSection) {
		error("Found `ENDL` outside of a `LOAD` block\n");
		return;
	}

	changeSection();
	curOffset += loadOffset;
	loadOffset = 0;
	currentLoadSection = nullptr;
	sym_SetCurrentSymbolScope(currentLoadScope);
}

Section *sect_GetSymbolSection() {
	return currentLoadSection ? currentLoadSection : currentSection;
}

// The offset into the section above
uint32_t sect_GetSymbolOffset() {
	return curOffset;
}

uint32_t sect_GetOutputOffset() {
	return curOffset + loadOffset;
}

// Returns how many bytes need outputting for the specified alignment and offset to succeed
uint32_t sect_GetAlignBytes(uint8_t alignment, uint16_t offset) {
	Section *sect = sect_GetSymbolSection();
	if (!sect)
		return 0;

	bool isFixed = sect->org != (uint32_t)-1;

	// If the section is not aligned, no bytes are needed
	// (fixed sections count as being maximally aligned for this purpose)
	uint8_t curAlignment = isFixed ? 16 : sect->align;
	if (curAlignment == 0)
		return 0;

	// We need `(pcValue + curOffset + return value) % (1 << alignment) == offset`
	uint16_t pcValue = isFixed ? sect->org : sect->alignOfs;
	return static_cast<uint16_t>(offset - curOffset - pcValue)
	       % (1u << std::min(alignment, curAlignment));
}

void sect_AlignPC(uint8_t alignment, uint16_t offset) {
	if (!checksection())
		return;

	Section *sect = sect_GetSymbolSection();
	uint32_t alignSize = 1 << alignment; // Size of an aligned "block"

	if (sect->org != (uint32_t)-1) {
		if ((sect->org + curOffset - offset) % alignSize)
			error(
			    "Section's fixed address fails required alignment (PC = $%04" PRIx32 ")\n",
			    sect->org + curOffset
			);
	} else if (sect->align != 0 && (((sect->alignOfs + curOffset) % (1u << sect->align)) - offset) % alignSize) {
		error(
		    "Section's alignment fails required alignment (offset from section start = $%04" PRIx32
		    ")\n",
		    curOffset
		);
	} else if (alignment >= 16) {
		// Treat an alignment large enough as fixing the address.
		// Note that this also ensures that a section's alignment never becomes 16 or greater.
		if (alignment > 16) {
			error("Alignment must be between 0 and 16, not %u\n", alignment);
		}
		sect->align = 0; // Reset the alignment, since we're fixing the address.
		sect->org = offset - curOffset;
	} else if (alignment > sect->align) {
		sect->align = alignment;
		// We need `(sect->alignOfs + curOffset) % alignSize == offset`
		sect->alignOfs = (offset - curOffset) % alignSize;
	}
}

static void growSection(uint32_t growth) {
	curOffset += growth;
	if (curOffset + loadOffset > currentSection->size)
		currentSection->size = curOffset + loadOffset;
	if (currentLoadSection && curOffset > currentLoadSection->size)
		currentLoadSection->size = curOffset;
}

static void writebyte(uint8_t byte) {
	currentSection->data[sect_GetOutputOffset()] = byte;
	growSection(1);
}

static void writeword(uint16_t b) {
	writebyte(b & 0xFF);
	writebyte(b >> 8);
}

static void writelong(uint32_t b) {
	writebyte(b & 0xFF);
	writebyte(b >> 8);
	writebyte(b >> 16);
	writebyte(b >> 24);
}

static void createPatch(PatchType type, Expression const &expr, uint32_t pcShift) {
	out_CreatePatch(type, expr, sect_GetOutputOffset(), pcShift);
}

void sect_StartUnion() {
	// Important info: currently, UNION and LOAD cannot interact, since UNION is prohibited in
	// "code" sections, whereas LOAD is restricted to them.
	// Therefore, any interactions are NOT TESTED, so lift either of those restrictions at
	// your own peril! ^^

	if (!currentSection) {
		error("UNIONs must be inside a SECTION\n");
		return;
	}
	if (sect_HasData(currentSection->type)) {
		error("Cannot use UNION inside of ROM0 or ROMX sections\n");
		return;
	}

	currentUnionStack.push({.start = curOffset, .size = 0});
}

static void endUnionMember() {
	UnionStackEntry &member = currentUnionStack.top();
	uint32_t memberSize = curOffset - member.start;

	if (memberSize > member.size)
		member.size = memberSize;
	curOffset = member.start;
}

void sect_NextUnionMember() {
	if (currentUnionStack.empty()) {
		error("Found NEXTU outside of a UNION construct\n");
		return;
	}
	endUnionMember();
}

void sect_EndUnion() {
	if (currentUnionStack.empty()) {
		error("Found ENDU outside of a UNION construct\n");
		return;
	}
	endUnionMember();
	curOffset += currentUnionStack.top().size;
	currentUnionStack.pop();
}

void sect_CheckUnionClosed() {
	if (!currentUnionStack.empty())
		error("Unterminated UNION construct\n");
}

// Output an absolute byte
void sect_AbsByte(uint8_t b) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(1))
		return;

	writebyte(b);
}

void sect_AbsByteString(std::vector<int32_t> const &s) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(s.size()))
		return;

	for (int32_t v : s) {
		if (!checkNBit(v, 8, "All character units"))
			break;
	}

	for (int32_t v : s)
		writebyte(static_cast<uint8_t>(v));
}

void sect_AbsWordString(std::vector<int32_t> const &s) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(s.size() * 2))
		return;

	for (int32_t v : s) {
		if (!checkNBit(v, 16, "All character units"))
			break;
	}

	for (int32_t v : s)
		writeword(static_cast<uint16_t>(v));
}

void sect_AbsLongString(std::vector<int32_t> const &s) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(s.size() * 4))
		return;

	for (int32_t v : s)
		writelong(static_cast<uint32_t>(v));
}

// Skip this many bytes
void sect_Skip(uint32_t skip, bool ds) {
	if (!checksection())
		return;
	if (!reserveSpace(skip))
		return;

	if (!sect_HasData(currentSection->type)) {
		growSection(skip);
	} else {
		if (!ds)
			warning(
			    WARNING_EMPTY_DATA_DIRECTIVE,
			    "%s directive without data in ROM\n",
			    (skip == 4)   ? "DL"
			    : (skip == 2) ? "DW"
			                  : "DB"
			);
		// We know we're in a code SECTION
		while (skip--)
			writebyte(fillByte);
	}
}

// Output a relocatable byte. Checking will be done to see if it
// is an absolute value in disguise.
void sect_RelByte(Expression &expr, uint32_t pcShift) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(1))
		return;

	if (!expr.isKnown()) {
		createPatch(PATCHTYPE_BYTE, expr, pcShift);
		writebyte(0);
	} else {
		writebyte(expr.value());
	}
}

// Output several copies of a relocatable byte. Checking will be done to see if
// it is an absolute value in disguise.
void sect_RelBytes(uint32_t n, std::vector<Expression> &exprs) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(n))
		return;

	for (uint32_t i = 0; i < n; i++) {
		Expression &expr = exprs[i % exprs.size()];

		if (!expr.isKnown()) {
			createPatch(PATCHTYPE_BYTE, expr, i);
			writebyte(0);
		} else {
			writebyte(expr.value());
		}
	}
}

// Output a relocatable word. Checking will be done to see if
// it's an absolute value in disguise.
void sect_RelWord(Expression &expr, uint32_t pcShift) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(2))
		return;

	if (!expr.isKnown()) {
		createPatch(PATCHTYPE_WORD, expr, pcShift);
		writeword(0);
	} else {
		writeword(expr.value());
	}
}

// Output a relocatable longword. Checking will be done to see if
// is an absolute value in disguise.
void sect_RelLong(Expression &expr, uint32_t pcShift) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(2))
		return;

	if (!expr.isKnown()) {
		createPatch(PATCHTYPE_LONG, expr, pcShift);
		writelong(0);
	} else {
		writelong(expr.value());
	}
}

// Output a PC-relative relocatable byte. Checking will be done to see if it
// is an absolute value in disguise.
void sect_PCRelByte(Expression &expr, uint32_t pcShift) {
	if (!checkcodesection())
		return;
	if (!reserveSpace(1))
		return;
	Symbol const *pc = sym_GetPC();

	if (!expr.isDiffConstant(pc)) {
		createPatch(PATCHTYPE_JR, expr, pcShift);
		writebyte(0);
	} else {
		Symbol const *sym = expr.symbolOf();
		// The offset wraps (jump from ROM to HRAM, for example)
		int16_t offset;

		// Offset is relative to the byte *after* the operand
		if (sym == pc)
			offset = -2; // PC as operand to `jr` is lower than reference PC by 2
		else
			offset = sym->getValue() - (pc->getValue() + 1);

		if (offset < -128 || offset > 127) {
			error(
			    "jr target must be between -128 and 127 bytes away, not %" PRId16
			    "; use jp instead\n",
			    offset
			);
			writebyte(0);
		} else {
			writebyte(offset);
		}
	}
}

// Output a binary file
void sect_BinaryFile(std::string const &name, int32_t startPos) {
	if (startPos < 0) {
		error("Start position cannot be negative (%" PRId32 ")\n", startPos);
		startPos = 0;
	}
	if (!checkcodesection())
		return;

	FILE *file = nullptr;
	if (std::optional<std::string> fullPath = fstk_FindFile(name); fullPath)
		file = fopen(fullPath->c_str(), "rb");
	if (!file) {
		if (generatedMissingIncludes) {
			if (verbose)
				printf("Aborting (-MG) on INCBIN file '%s' (%s)\n", name.c_str(), strerror(errno));
			failedOnMissingInclude = true;
		} else {
			error("Error opening INCBIN file '%s': %s\n", name.c_str(), strerror(errno));
		}
		return;
	}
	Defer closeFile{[&] { fclose(file); }};

	int32_t fsize = -1;

	if (fseek(file, 0, SEEK_END) != -1) {
		fsize = ftell(file);

		if (startPos > fsize) {
			error("Specified start position is greater than length of file\n");
			return;
		}

		fseek(file, startPos, SEEK_SET);

		if (!reserveSpace(fsize - startPos))
			return;
	} else {
		if (errno != ESPIPE)
			error(
			    "Error determining size of INCBIN file '%s': %s\n", name.c_str(), strerror(errno)
			);
		// The file isn't seekable, so we'll just skip bytes
		while (startPos--)
			(void)fgetc(file);
	}

	for (int byte; (byte = fgetc(file)) != EOF;) {
		if (fsize == -1)
			growSection(1);
		writebyte(byte);
	}

	if (ferror(file))
		error("Error reading INCBIN file '%s': %s\n", name.c_str(), strerror(errno));
}

// Output a slice of a binary file
void sect_BinaryFileSlice(std::string const &name, int32_t startPos, int32_t length) {
	if (startPos < 0) {
		error("Start position cannot be negative (%" PRId32 ")\n", startPos);
		startPos = 0;
	}

	if (length < 0) {
		error("Number of bytes to read cannot be negative (%" PRId32 ")\n", length);
		length = 0;
	}

	if (!checkcodesection())
		return;
	if (length == 0) // Don't even bother with 0-byte slices
		return;
	if (!reserveSpace(length))
		return;

	FILE *file = nullptr;
	if (std::optional<std::string> fullPath = fstk_FindFile(name); fullPath)
		file = fopen(fullPath->c_str(), "rb");
	if (!file) {
		if (generatedMissingIncludes) {
			if (verbose)
				printf("Aborting (-MG) on INCBIN file '%s' (%s)\n", name.c_str(), strerror(errno));
			failedOnMissingInclude = true;
		} else {
			error("Error opening INCBIN file '%s': %s\n", name.c_str(), strerror(errno));
		}
		return;
	}
	Defer closeFile{[&] { fclose(file); }};

	if (fseek(file, 0, SEEK_END) != -1) {
		int32_t fsize = ftell(file);

		if (startPos > fsize) {
			error("Specified start position is greater than length of file\n");
			return;
		}

		if ((startPos + length) > fsize) {
			error(
			    "Specified range in INCBIN is out of bounds (%" PRIu32 " + %" PRIu32 " > %" PRIu32
			    ")\n",
			    startPos,
			    length,
			    fsize
			);
			return;
		}

		fseek(file, startPos, SEEK_SET);
	} else {
		if (errno != ESPIPE)
			error(
			    "Error determining size of INCBIN file '%s': %s\n", name.c_str(), strerror(errno)
			);
		// The file isn't seekable, so we'll just skip bytes
		while (startPos--)
			(void)fgetc(file);
	}

	while (length--) {
		int byte = fgetc(file);

		if (byte != EOF) {
			writebyte(byte);
		} else if (ferror(file)) {
			error("Error reading INCBIN file '%s': %s\n", name.c_str(), strerror(errno));
		} else {
			error("Premature end of file (%" PRId32 " bytes left to read)\n", length + 1);
		}
	}
}

// Section stack routines
void sect_PushSection() {
	sectionStack.push_front({
	    .section = currentSection,
	    .loadSection = currentLoadSection,
	    .scope = sym_GetCurrentSymbolScope(),
	    .offset = curOffset,
	    .loadOffset = loadOffset,
	    .unionStack = {},
	});

	// Reset the section scope
	currentSection = nullptr;
	currentLoadSection = nullptr;
	sym_SetCurrentSymbolScope(std::nullopt);
	std::swap(currentUnionStack, sectionStack.front().unionStack);
}

void sect_PopSection() {
	if (sectionStack.empty())
		fatalerror("No entries in the section stack\n");

	if (currentLoadSection)
		fatalerror("Cannot change the section within a `LOAD` block\n");

	SectionStackEntry entry = sectionStack.front();
	sectionStack.pop_front();

	changeSection();
	currentSection = entry.section;
	currentLoadSection = entry.loadSection;
	sym_SetCurrentSymbolScope(entry.scope);
	curOffset = entry.offset;
	loadOffset = entry.loadOffset;
	std::swap(currentUnionStack, entry.unionStack);
}

void sect_EndSection() {
	if (!currentSection)
		fatalerror("Cannot end the section outside of a SECTION\n");

	if (currentLoadSection)
		fatalerror("Cannot end the section within a `LOAD` block\n");

	if (!currentUnionStack.empty())
		fatalerror("Cannot end the section within a UNION\n");

	// Reset the section scope
	currentSection = nullptr;
	sym_SetCurrentSymbolScope(std::nullopt);
}
