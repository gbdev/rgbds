// SPDX-License-Identifier: MIT

#include "link/output.hpp"

#include <algorithm>
#include <deque>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "error.hpp"
#include "extern/utf8decoder.hpp"
#include "helpers.hpp"
#include "linkdefs.hpp"
#include "platform.hpp"

#include "link/main.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"

static constexpr size_t BANK_SIZE = 0x4000;

FILE *outputFile;
FILE *overlayFile;
FILE *symFile;
FILE *mapFile;

struct SortedSymbol {
	Symbol const *sym;
	uint16_t addr;
	uint16_t parentAddr;
};

struct SortedSections {
	std::deque<Section const *> sections;
	std::deque<Section const *> zeroLenSections;
};

static std::deque<SortedSections> sections[SECTTYPE_INVALID];

// Defines the order in which types are output to the sym and map files
static SectionType typeMap[SECTTYPE_INVALID] = {
    SECTTYPE_ROM0,
    SECTTYPE_ROMX,
    SECTTYPE_VRAM,
    SECTTYPE_SRAM,
    SECTTYPE_WRAM0,
    SECTTYPE_WRAMX,
    SECTTYPE_OAM,
    SECTTYPE_HRAM,
};

void out_AddSection(Section const &section) {
	static uint32_t const maxNbBanks[SECTTYPE_INVALID] = {
	    1,          // SECTTYPE_WRAM0
	    2,          // SECTTYPE_VRAM
	    UINT32_MAX, // SECTTYPE_ROMX
	    1,          // SECTTYPE_ROM0
	    1,          // SECTTYPE_HRAM
	    7,          // SECTTYPE_WRAMX
	    UINT32_MAX, // SECTTYPE_SRAM
	    1,          // SECTTYPE_OAM
	};

	uint32_t targetBank = section.bank - sectionTypeInfo[section.type].firstBank;
	uint32_t minNbBanks = targetBank + 1;

	if (minNbBanks > maxNbBanks[section.type]) {
		errx(
		    "Section \"%s\" has an invalid bank range (%" PRIu32 " > %" PRIu32 ")",
		    section.name.c_str(),
		    section.bank,
		    maxNbBanks[section.type] - 1
		);
	}

	for (uint32_t i = sections[section.type].size(); i < minNbBanks; i++) {
		sections[section.type].emplace_back();
	}

	std::deque<Section const *> &bankSections =
	    section.size ? sections[section.type][targetBank].sections
	                 : sections[section.type][targetBank].zeroLenSections;
	auto pos = bankSections.begin();

	while (pos != bankSections.end() && (*pos)->org < section.org) {
		pos++;
	}

	bankSections.insert(pos, &section);
}

Section const *out_OverlappingSection(Section const &section) {
	uint32_t bank = section.bank - sectionTypeInfo[section.type].firstBank;

	for (Section const *ptr : sections[section.type][bank].sections) {
		if (ptr->org < section.org + section.size && section.org < ptr->org + ptr->size) {
			return ptr;
		}
	}
	return nullptr;
}

// Performs sanity checks on the overlay file.
// Returns the number of ROM banks in the overlay file.
static uint32_t checkOverlaySize() {
	if (!overlayFile) {
		return 0;
	}

	if (fseek(overlayFile, 0, SEEK_END) != 0) {
		warnx("Overlay file is not seekable, cannot check if properly formed");
		return 0;
	}

	long overlaySize = ftell(overlayFile);

	// Reset back to beginning
	fseek(overlayFile, 0, SEEK_SET);

	if (overlaySize % BANK_SIZE) {
		warnx("Overlay file does not have a size multiple of 0x4000");
	} else if (is32kMode && overlaySize != 0x8000) {
		warnx("Overlay is not exactly 0x8000 bytes large");
	} else if (overlaySize < 0x8000) {
		warnx("Overlay is less than 0x8000 bytes large");
	}

	return (overlaySize + BANK_SIZE - 1) / BANK_SIZE;
}

// Expand `sections[SECTTYPE_ROMX]` to cover all the overlay banks.
// This ensures that `writeROM` will output each bank, even if some are not
// covered by any sections.
static void coverOverlayBanks(uint32_t nbOverlayBanks) {
	// 2 if is32kMode, 1 otherwise
	uint32_t nbRom0Banks = sectionTypeInfo[SECTTYPE_ROM0].size / BANK_SIZE;
	// Discount ROM0 banks to avoid outputting too much
	uint32_t nbUncoveredBanks = nbOverlayBanks - nbRom0Banks > sections[SECTTYPE_ROMX].size()
	                                ? nbOverlayBanks - nbRom0Banks
	                                : 0;

	if (nbUncoveredBanks > sections[SECTTYPE_ROMX].size()) {
		for (uint32_t i = sections[SECTTYPE_ROMX].size(); i < nbUncoveredBanks; i++) {
			sections[SECTTYPE_ROMX].emplace_back();
		}
	}
}

static uint8_t getNextFillByte() {
	if (overlayFile) {
		int c = getc(overlayFile);
		if (c != EOF) {
			return c;
		}

		if (static bool warned = false; !hasPadValue && !warned) {
			warnx("Output is larger than overlay file, but no padding value was specified");
			warned = true;
		}
	}

	return padValue;
}

// Write a ROM bank's sections, ordered by increasing address, to the output file.
static void
    writeBank(std::deque<Section const *> *bankSections, uint16_t baseOffset, uint16_t size) {
	uint16_t offset = 0;

	if (bankSections) {
		for (Section const *section : *bankSections) {
			assume(section->offset == 0);
			// Output padding up to the next SECTION
			while (offset + baseOffset < section->org) {
				putc(getNextFillByte(), outputFile);
				offset++;
			}

			// Output the section itself
			fwrite(section->data.data(), 1, section->size, outputFile);
			if (overlayFile) {
				// Skip bytes even with pipes
				for (uint16_t i = 0; i < section->size; i++) {
					getc(overlayFile);
				}
			}
			offset += section->size;
		}
	}

	if (!disablePadding) {
		while (offset < size) {
			putc(getNextFillByte(), outputFile);
			offset++;
		}
	}
}

// Writes a ROM file to the output.
static void writeROM() {
	if (outputFileName) {
		if (strcmp(outputFileName, "-")) {
			outputFile = fopen(outputFileName, "wb");
		} else {
			outputFileName = "<stdout>";
			(void)setmode(STDOUT_FILENO, O_BINARY);
			outputFile = stdout;
		}
		if (!outputFile) {
			err("Failed to open output file \"%s\"", outputFileName);
		}
	}
	Defer closeOutputFile{[&] {
		if (outputFile) {
			fclose(outputFile);
		}
	}};

	if (overlayFileName) {
		if (strcmp(overlayFileName, "-")) {
			overlayFile = fopen(overlayFileName, "rb");
		} else {
			overlayFileName = "<stdin>";
			(void)setmode(STDIN_FILENO, O_BINARY);
			overlayFile = stdin;
		}
		if (!overlayFile) {
			err("Failed to open overlay file \"%s\"", overlayFileName);
		}
	}
	Defer closeOverlayFile{[&] {
		if (overlayFile) {
			fclose(overlayFile);
		}
	}};

	uint32_t nbOverlayBanks = checkOverlaySize();

	if (nbOverlayBanks > 0) {
		coverOverlayBanks(nbOverlayBanks);
	}

	if (outputFile) {
		writeBank(
		    !sections[SECTTYPE_ROM0].empty() ? &sections[SECTTYPE_ROM0][0].sections : nullptr,
		    sectionTypeInfo[SECTTYPE_ROM0].startAddr,
		    sectionTypeInfo[SECTTYPE_ROM0].size
		);

		for (uint32_t i = 0; i < sections[SECTTYPE_ROMX].size(); i++) {
			writeBank(
			    &sections[SECTTYPE_ROMX][i].sections,
			    sectionTypeInfo[SECTTYPE_ROMX].startAddr,
			    sectionTypeInfo[SECTTYPE_ROMX].size
			);
		}
	}
}

// Checks whether a symbol is legal for a sym file or map file.
// Eliminates anonymous labels, which start with a '!'.
static bool isLegalSymbol(Symbol const &sym) {
	if (sym.name.empty()) {
		return false;
	}
	char c = sym.name[0];
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

// Checks whether this character is legal in a symbol's name in a sym file
static bool isLegalForSymName(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'
	       || c == '@' || c == '#' || c == '$' || c == '.';
}

// Prints a symbol's name to a file, assuming that the first character is legal.
// Illegal characters are UTF-8-decoded (errors are replaced by U+FFFD) and emitted as '\u'/'\U'.
static void printSymName(std::string const &name, FILE *file) {
	for (char const *ptr = name.c_str(); *ptr != '\0';) {
		char c = *ptr;

		if (isLegalForSymName(c)) {
			// Output legal ASCII characters as-is
			putc(c, file);
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
					while ((*ptr & 0xC0) == 0x80) {
						++ptr;
					}
					break;
				}
				++ptr;
			} while (state != 0);

			fprintf(file, codepoint <= 0xFFFF ? "\\u%04" PRIx32 : "\\U%08" PRIx32, codepoint);
		}
	}
}

// Comparator function for `std::stable_sort` to sort symbols
static bool compareSymbols(SortedSymbol const &sym1, SortedSymbol const &sym2) {
	// First, sort by address
	if (sym1.addr != sym2.addr) {
		return sym1.addr < sym2.addr;
	}

	// Second, sort by locality (global before local)
	std::string const &sym1_name = sym1.sym->name;
	std::string const &sym2_name = sym2.sym->name;
	bool sym1_local = sym1_name.find('.') != std::string::npos;
	bool sym2_local = sym2_name.find('.') != std::string::npos;
	if (sym1_local != sym2_local) {
		return sym1_local < sym2_local;
	}

	// Third, sort by parent address
	if (sym1.parentAddr != sym2.parentAddr) {
		return sym1.parentAddr < sym2.parentAddr;
	}

	// Fourth, sort by name
	return sym1_name < sym2_name;
}

// Write a bank's contents to the sym file
static void writeSymBank(SortedSections const &bankSections, SectionType type, uint32_t bank) {
#define forEachSortedSection(sect, ...) \
	do { \
		for (auto it = bankSections.zeroLenSections.begin(); \
		     it != bankSections.zeroLenSections.end(); \
		     it++) { \
			for (Section const *sect = *it; sect; sect = sect->nextu.get()) { \
				__VA_ARGS__ \
			} \
		} \
		for (auto it = bankSections.sections.begin(); it != bankSections.sections.end(); it++) { \
			for (Section const *sect = *it; sect; sect = sect->nextu.get()) { \
				__VA_ARGS__ \
			} \
		} \
	} while (0)

	uint32_t nbSymbols = 0;

	forEachSortedSection(sect, { nbSymbols += sect->symbols.size(); });

	if (!nbSymbols) {
		return;
	}

	std::vector<SortedSymbol> symList;

	symList.reserve(nbSymbols);

	forEachSortedSection(sect, {
		for (Symbol const *sym : sect->symbols) {
			// Don't output symbols that begin with an illegal character
			if (isLegalSymbol(*sym)) {
				uint16_t addr = static_cast<uint16_t>(sym->label().offset + sect->org);
				uint16_t parentAddr = addr;
				if (auto pos = sym->name.find('.'); pos != std::string::npos) {
					std::string parentName = sym->name.substr(0, pos);
					if (Symbol const *parentSym = sym_GetSymbol(parentName);
					    parentSym && parentSym->data.holds<Label>()) {
						auto const &parentLabel = parentSym->label();
						assume(parentLabel.section != nullptr);
						parentAddr =
						    static_cast<uint16_t>(parentLabel.offset + parentLabel.section->org);
					}
				}
				symList.push_back({.sym = sym, .addr = addr, .parentAddr = parentAddr});
			}
		}
	});

#undef forEachSortedSection

	std::stable_sort(RANGE(symList), compareSymbols);

	uint32_t symBank = bank + sectionTypeInfo[type].firstBank;

	for (SortedSymbol &sym : symList) {
		fprintf(symFile, "%02" PRIx32 ":%04" PRIx16 " ", symBank, sym.addr);
		printSymName(sym.sym->name, symFile);
		putc('\n', symFile);
	}
}

static void writeEmptySpace(uint16_t begin, uint16_t end) {
	if (begin < end) {
		uint16_t len = end - begin;

		fprintf(
		    mapFile,
		    "\tEMPTY: $%04x-$%04x ($%04" PRIx16 " byte%s)\n",
		    begin,
		    end - 1,
		    len,
		    len == 1 ? "" : "s"
		);
	}
}

// Prints a section's name to a file.
static void printSectionName(std::string const &name, FILE *file) {
	for (char c : name) {
		// Escape characters that need escaping
		switch (c) {
		case '\n':
			fputs("\\n", file);
			break;
		case '\r':
			fputs("\\r", file);
			break;
		case '\t':
			fputs("\\t", file);
			break;
		case '\\':
		case '"':
			putc('\\', file);
			[[fallthrough]];
		default:
			putc(c, file);
			break;
		}
	}
}

// Write a bank's contents to the map file
static void writeMapBank(SortedSections const &sectList, SectionType type, uint32_t bank) {
	fprintf(
	    mapFile,
	    "\n%s bank #%" PRIu32 ":\n",
	    sectionTypeInfo[type].name.c_str(),
	    bank + sectionTypeInfo[type].firstBank
	);

	uint16_t used = 0;
	auto section = sectList.sections.begin();
	auto zeroLenSection = sectList.zeroLenSections.begin();
	uint16_t prevEndAddr = sectionTypeInfo[type].startAddr;

	while (section != sectList.sections.end() || zeroLenSection != sectList.zeroLenSections.end()) {
		// Pick the lowest section by address out of the two
		auto &pickedSection = section == sectList.sections.end()                 ? zeroLenSection
		                      : zeroLenSection == sectList.zeroLenSections.end() ? section
		                      : (*section)->org < (*zeroLenSection)->org         ? section
		                                                                         : zeroLenSection;
		Section const *sect = *pickedSection;

		used += sect->size;
		assume(sect->offset == 0);

		writeEmptySpace(prevEndAddr, sect->org);

		prevEndAddr = sect->org + sect->size;

		fprintf(mapFile, "\tSECTION: $%04" PRIx16, sect->org);
		if (sect->size != 0) {
			fprintf(mapFile, "-$%04x", prevEndAddr - 1);
		}
		fprintf(mapFile, " ($%04" PRIx16 " byte%s) [\"", sect->size, sect->size == 1 ? "" : "s");
		printSectionName(sect->name, mapFile);
		fputs("\"]\n", mapFile);

		if (!noSymInMap) {
			// Also print symbols in the following "pieces"
			for (uint16_t org = sect->org; sect; sect = sect->nextu.get()) {
				for (Symbol *sym : sect->symbols) {
					// Don't output symbols that begin with an illegal character
					if (isLegalSymbol(*sym)) {
						// Space matches "\tSECTION: $xxxx ..."
						fprintf(mapFile, "\t         $%04" PRIx32 " = ", sym->label().offset + org);
						printSymName(sym->name, mapFile);
						putc('\n', mapFile);
					}
				}

				if (sect->nextu) {
					// Announce the following "piece"
					if (sect->nextu->modifier == SECTION_UNION) {
						fprintf(mapFile, "\t         ; Next union\n");
					} else if (sect->nextu->modifier == SECTION_FRAGMENT) {
						fprintf(mapFile, "\t         ; Next fragment\n");
					}
				}
			}
		}

		pickedSection++;
	}

	if (used == 0) {
		fputs("\tEMPTY\n", mapFile);
	} else {
		uint16_t bankEndAddr = sectionTypeInfo[type].startAddr + sectionTypeInfo[type].size;

		writeEmptySpace(prevEndAddr, bankEndAddr);

		uint16_t slack = sectionTypeInfo[type].size - used;

		fprintf(mapFile, "\tTOTAL EMPTY: $%04" PRIx16 " byte%s\n", slack, slack == 1 ? "" : "s");
	}
}

// Write the total used and free space by section type to the map file
static void writeMapSummary() {
	fputs("SUMMARY:\n", mapFile);

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		SectionType type = typeMap[i];
		uint32_t nbBanks = sections[type].size();

		// Do not output used space for VRAM or OAM
		if (type == SECTTYPE_VRAM || type == SECTTYPE_OAM) {
			continue;
		}

		// Do not output unused section types
		if (nbBanks == 0) {
			continue;
		}

		uint32_t usedTotal = 0;

		for (uint32_t bank = 0; bank < nbBanks; bank++) {
			uint16_t used = 0;
			auto &sectList = sections[type][bank];
			auto section = sectList.sections.begin();
			auto zeroLenSection = sectList.zeroLenSections.begin();

			while (section != sectList.sections.end()
			       || zeroLenSection != sectList.zeroLenSections.end()) {
				// Pick the lowest section by address out of the two
				auto &pickedSection = section == sectList.sections.end() ? zeroLenSection
				                      : zeroLenSection == sectList.zeroLenSections.end() ? section
				                      : (*section)->org < (*zeroLenSection)->org         ? section
				                                                                 : zeroLenSection;

				used += (*pickedSection)->size;
				pickedSection++;
			}

			usedTotal += used;
		}

		fprintf(
		    mapFile,
		    "\t%s: %" PRId32 " byte%s used / %" PRId32 " free",
		    sectionTypeInfo[type].name.c_str(),
		    usedTotal,
		    usedTotal == 1 ? "" : "s",
		    nbBanks * sectionTypeInfo[type].size - usedTotal
		);
		if (sectionTypeInfo[type].firstBank != sectionTypeInfo[type].lastBank || nbBanks > 1) {
			fprintf(mapFile, " in %u bank%s", nbBanks, nbBanks == 1 ? "" : "s");
		}
		putc('\n', mapFile);
	}
}

// Writes the sym file, if applicable.
static void writeSym() {
	if (!symFileName) {
		return;
	}

	if (strcmp(symFileName, "-")) {
		symFile = fopen(symFileName, "w");
	} else {
		symFileName = "<stdout>";
		(void)setmode(STDOUT_FILENO, O_TEXT); // May have been set to O_BINARY previously
		symFile = stdout;
	}
	if (!symFile) {
		err("Failed to open sym file \"%s\"", symFileName);
	}
	Defer closeSymFile{[&] { fclose(symFile); }};

	fputs("; File generated by rgblink\n", symFile);

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		SectionType type = typeMap[i];

		for (uint32_t bank = 0; bank < sections[type].size(); bank++) {
			writeSymBank(sections[type][bank], type, bank);
		}
	}

	// Output the exported numeric constants
	static std::vector<Symbol *> constants; // `static` so `sym_ForEach` callback can see it
	constants.clear();
	sym_ForEach([](Symbol &sym) {
		// Symbols are already limited to the exported ones
		if (sym.data.holds<int32_t>()) {
			constants.push_back(&sym);
		}
	});
	// Numeric constants are ordered by value, then by name
	std::sort(RANGE(constants), [](Symbol *sym1, Symbol *sym2) -> bool {
		int32_t val1 = sym1->data.get<int32_t>(), val2 = sym2->data.get<int32_t>();
		return val1 != val2 ? val1 < val2 : sym1->name < sym2->name;
	});
	for (Symbol *sym : constants) {
		int32_t val = sym->data.get<int32_t>();
		int width = val < 0x100 ? 2 : val < 0x10000 ? 4 : 8;
		fprintf(symFile, "%0*" PRIx32 " ", width, val);
		printSymName(sym->name, symFile);
		putc('\n', symFile);
	}
}

// Writes the map file, if applicable.
static void writeMap() {
	if (!mapFileName) {
		return;
	}

	if (strcmp(mapFileName, "-")) {
		mapFile = fopen(mapFileName, "w");
	} else {
		mapFileName = "<stdout>";
		(void)setmode(STDOUT_FILENO, O_TEXT); // May have been set to O_BINARY previously
		mapFile = stdout;
	}
	if (!mapFile) {
		err("Failed to open map file \"%s\"", mapFileName);
	}
	Defer closeMapFile{[&] { fclose(mapFile); }};

	writeMapSummary();

	for (uint8_t i = 0; i < SECTTYPE_INVALID; i++) {
		SectionType type = typeMap[i];

		for (uint32_t bank = 0; bank < sections[type].size(); bank++) {
			writeMapBank(sections[type][bank], type, bank);
		}
	}
}

void out_WriteFiles() {
	writeROM();
	writeSym();
	writeMap();
}
