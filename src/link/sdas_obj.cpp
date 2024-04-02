/* SPDX-License-Identifier: MIT */

#include "link/sdas_obj.hpp"

#include <ctype.h>
#include <inttypes.h>
#include <memory>
#include <stdint.h>
#include <string.h>
#include <tuple>
#include <variant>

#include "helpers.hpp" // assume
#include "linkdefs.hpp"
#include "platform.hpp"

#include "link/assign.hpp"
#include "link/main.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"

enum NumberType {
	HEX = 16, // X
	DEC = 10, // D
	OCT = 8,  // Q
};

static void consumeLF(FileStackNode const &where, uint32_t lineNo, FILE *file) {
	if (getc(file) != '\n')
		fatal(&where, lineNo, "Bad line ending (CR without LF)");
}

static char const *delim = " \f\n\r\t\v"; // Whitespace according to the C and POSIX locales

static int
    nextLine(std::vector<char> &lineBuf, uint32_t &lineNo, FileStackNode const &where, FILE *file) {
retry:
	++lineNo;
	int firstChar = getc(file);

	switch (firstChar) {
	case EOF:
		return EOF;
	case ';':
		// Discard comment line
		// TODO: if `;!FILE [...]` on the first line (`lineNo`), return it
		do {
			firstChar = getc(file);
		} while (firstChar != EOF && firstChar != '\r' && firstChar != '\n');
		[[fallthrough]];
	case '\r':
		if (firstChar == '\r' && getc(file) != '\n')
			consumeLF(where, lineNo, file);
		[[fallthrough]];
	case '\n':
		goto retry;
	}

	for (;;) {
		int c = getc(file);

		switch (c) {
		case '\r':
			consumeLF(where, lineNo, file);
			[[fallthrough]];
		case '\n':
		case EOF:
			lineBuf.push_back('\0'); // Terminate the string (space was ensured above)
			return firstChar;
		}
		lineBuf.push_back(c);
	}
}

static uint32_t readNumber(char const *str, char const *&endptr, NumberType base) {
	uint32_t res = 0;

	for (;;) {
		static char const *digits = "0123456789ABCDEF";
		char const *ptr = strchr(digits, toupper(*str));

		if (!ptr || ptr - digits >= base) {
			endptr = str;
			return res;
		}
		++str;
		res = res * base + (ptr - digits);
	}
}

static uint32_t
    parseNumber(FileStackNode const &where, uint32_t lineNo, char const *str, NumberType base) {
	if (str[0] == '\0')
		fatal(&where, lineNo, "Expected number, got empty string");

	char const *endptr;
	uint32_t res = readNumber(str, endptr, base);

	if (*endptr != '\0')
		fatal(&where, lineNo, "Expected number, got \"%s\"", str);
	return res;
}

static uint8_t
    parseByte(FileStackNode const &where, uint32_t lineNo, char const *str, NumberType base) {
	uint32_t num = parseNumber(where, lineNo, str, base);

	if (num > UINT8_MAX)
		fatal(&where, lineNo, "\"%s\" is not a byte", str);
	return num;
}

enum AreaFlags {
	AREA_TYPE = 2, // 0: Concatenate, 1: overlay
	AREA_ISABS,    // 0: Relative (???) address, 1: absolute address
	AREA_PAGING,   // Unsupported

	AREA_ALL_FLAGS = 1 << AREA_TYPE | 1 << AREA_ISABS | 1 << AREA_PAGING,
};

enum RelocFlags {
	RELOC_SIZE,      // 0: 16-bit, 1: 8-bit
	RELOC_ISSYM,     // 0: Area, 1: Symbol
	RELOC_ISPCREL,   // 0: Normal, 1: PC-relative
	RELOC_EXPR16,    // Only for 8-bit size; 0: 8-bit expr, 1: 16-bit expr
	RELOC_SIGNED,    // 0: signed, 1: unsigned
	RELOC_ZPAGE,     // Unsupported
	RELOC_NPAGE,     // Unsupported
	RELOC_WHICHBYTE, // 8-bit size with 16-bit expr only; 0: LOW(), 1: HIGH()
	RELOC_EXPR24,    // Only for 8-bit size; 0: follow RELOC_EXPR16, 1: 24-bit expr
	RELOC_BANKBYTE,  // 8-bit size with 24-bit expr only; 0: follow RELOC_WHICHBYTE, 1: BANK()

	RELOC_ALL_FLAGS = 1 << RELOC_SIZE | 1 << RELOC_ISSYM | 1 << RELOC_ISPCREL | 1 << RELOC_EXPR16
	                  | 1 << RELOC_SIGNED | 1 << RELOC_ZPAGE | 1 << RELOC_NPAGE
	                  | 1 << RELOC_WHICHBYTE | 1 << RELOC_EXPR24 | 1 << RELOC_BANKBYTE,
};

void sdobj_ReadFile(FileStackNode const &where, FILE *file, std::vector<Symbol> &fileSymbols) {
	std::vector<char> line(256);
	char const *token;

#define getToken(ptr, ...) \
	do { \
		token = strtok((ptr), delim); \
		if (!token) \
			fatal(&where, lineNo, __VA_ARGS__); \
	} while (0)
#define expectEol(...) \
	do { \
		token = strtok(nullptr, delim); \
		if (token) \
			fatal(&where, lineNo, __VA_ARGS__); \
	} while (0)
#define expectToken(expected, lineType) \
	do { \
		getToken(nullptr, "'%c' line is too short", (lineType)); \
		if (strcasecmp(token, (expected)) != 0) \
			fatal( \
			    &where, \
			    lineNo, \
			    "Malformed '%c' line: expected \"%s\", got \"%s\"", \
			    (lineType), \
			    (expected), \
			    token \
			); \
	} while (0)

	uint32_t lineNo = 0;
	int lineType = nextLine(line, lineNo, where, file);
	NumberType numberType;

	// The first letter (thus, the line type) identifies the integer type
	switch (lineType) {
	case EOF:
		fatal(&where, lineNo, "SDCC object only contains comments and empty lines");
	case 'X':
		numberType = HEX;
		break;
	case 'D':
		numberType = DEC;
		break;
	case 'Q':
		numberType = OCT;
		break;
	default:
		fatal(
		    &where,
		    lineNo,
		    "This does not look like a SDCC object file (unknown integer format '%c')",
		    lineType
		);
	}

	switch (line[0]) {
	case 'L':
		break;
	case 'H':
		fatal(&where, lineNo, "Big-endian SDCC object files are not supported");
	default:
		fatal(&where, lineNo, "Unknown endianness type '%c'", line[0]);
	}

#define ADDR_SIZE 3
	if (line[1] != '0' + ADDR_SIZE)
		fatal(&where, lineNo, "Unknown or unsupported address size '%c'", line[1]);

	if (line[2] != '\0')
		warning(&where, lineNo, "Ignoring unknown characters (\"%s\") in first line", &line[2]);

	// Header line

	lineType = nextLine(line, lineNo, where, file);
	if (lineType != 'H')
		fatal(&where, lineNo, "Expected header line, got '%c' line", lineType);
	// Expected format: "A areas S global symbols"

	getToken(line.data(), "Empty 'H' line");
	uint32_t expectedNbAreas = parseNumber(where, lineNo, token, numberType);

	expectToken("areas", 'H');

	getToken(nullptr, "'H' line is too short");
	uint32_t expectedNbSymbols = parseNumber(where, lineNo, token, numberType);

	expectToken("global", 'H');

	expectToken("symbols", 'H');

	expectEol("'H' line is too long");

	// Now, let's parse the rest of the lines as they come!

	struct FileSection {
		std::unique_ptr<Section> section;
		uint16_t writeIndex;
	};
	std::vector<FileSection> fileSections;
	std::vector<uint8_t> data;

	for (;;) {
		lineType = nextLine(line, lineNo, where, file);
		if (lineType == EOF)
			break;
		switch (lineType) {
		case 'M': // Module name
		case 'O': // Assembler flags
			// Ignored
			break;

		case 'A': {
			if (fileSections.size() == expectedNbAreas)
				warning(
				    &where, lineNo, "Got more 'A' lines than the expected %" PRIu32, expectedNbAreas
				);
			std::unique_ptr<Section> curSection = std::make_unique<Section>();

			getToken(line.data(), "'A' line is too short");
			assume(strlen(token) != 0); // This should be impossible, tokens are non-empty
			// The following is required for fragment offsets to be reliably predicted
			for (FileSection &entry : fileSections) {
				if (!strcmp(token, entry.section->name.c_str()))
					fatal(&where, lineNo, "Area \"%s\" already defined earlier", token);
			}
			char const *sectName = token; // We'll deal with the section's name depending on type

			expectToken("size", 'A');

			getToken(nullptr, "'A' line is too short");

			uint32_t tmp = parseNumber(where, lineNo, token, numberType);

			if (tmp > UINT16_MAX)
				fatal(
				    &where,
				    lineNo,
				    "Area \"%s\" is larger than the GB address space!?",
				    curSection->name.c_str()
				);
			curSection->size = tmp;

			expectToken("flags", 'A');

			getToken(nullptr, "'A' line is too short");
			tmp = parseNumber(where, lineNo, token, numberType);
			if (tmp & (1 << AREA_PAGING))
				fatal(&where, lineNo, "Internal error: paging is not supported");
			curSection->isAddressFixed = tmp & (1 << AREA_ISABS);
			curSection->isBankFixed = curSection->isAddressFixed;
			curSection->modifier = curSection->isAddressFixed || (tmp & (1 << AREA_TYPE))
			                           ? SECTION_NORMAL
			                           : SECTION_FRAGMENT;
			// If the section is absolute, its name might not be unique; thus, mangle the name
			if (curSection->modifier == SECTION_NORMAL) {
				curSection->name.append(where.name());
				curSection->name.append(" ");
			}
			curSection->name.append(sectName);

			expectToken("addr", 'A');

			getToken(nullptr, "'A' line is too short");
			tmp = parseNumber(where, lineNo, token, numberType);
			curSection->org = tmp; // Truncation keeps the address portion only
			curSection->bank = tmp >> 16;

			expectEol("'A' line is too long");

			// Init the rest of the members
			curSection->offset = 0;
			if (curSection->isAddressFixed) {
				uint8_t high = curSection->org >> 8;

				if (high < 0x40) {
					curSection->type = SECTTYPE_ROM0;
				} else if (high < 0x80) {
					curSection->type = SECTTYPE_ROMX;
				} else if (high < 0xA0) {
					curSection->type = SECTTYPE_VRAM;
				} else if (high < 0xC0) {
					curSection->type = SECTTYPE_SRAM;
				} else if (high < 0xD0) {
					curSection->type = SECTTYPE_WRAM0;
				} else if (high < 0xE0) {
					curSection->type = SECTTYPE_WRAMX;
				} else if (high < 0xFE) {
					fatal(&where, lineNo, "Areas in echo RAM are not supported");
				} else if (high < 0xFF) {
					curSection->type = SECTTYPE_OAM;
				} else {
					curSection->type = SECTTYPE_HRAM;
				}
			} else {
				curSection->type = SECTTYPE_INVALID; // This means "indeterminate"
			}
			curSection->isAlignFixed = false;       // No such concept!
			curSection->fileSymbols = &fileSymbols; // IDs are instead per-section
			curSection->nextu = nullptr;

			fileSections.push_back({.section = std::move(curSection), .writeIndex = 0});
			break;
		}

		case 'S': {
			if (fileSymbols.size() == expectedNbSymbols)
				warning(
				    &where,
				    lineNo,
				    "Got more 'S' lines than the expected %" PRIu32,
				    expectedNbSymbols
				);
			Symbol &symbol = fileSymbols.emplace_back();

			// Init other members
			symbol.objFileName = where.name().c_str();
			symbol.src = &where;
			symbol.lineNo = lineNo;

			getToken(line.data(), "'S' line is too short");
			symbol.name = token;

			getToken(nullptr, "'S' line is too short");

			if (int32_t value = parseNumber(where, lineNo, &token[3], numberType);
			    !fileSections.empty()) {
				// Symbols in sections are labels; their value is an offset
				Section *section = fileSections.back().section.get();
				if (section->isAddressFixed) {
					assume(value >= section->org && value <= section->org + section->size);
					value -= section->org;
				}
				// No need to set the `sectionID`, since we set the pointer
				symbol.data = Label{.sectionID = 0, .offset = value, .section = section};
			} else {
				// Symbols without sections are just constants
				symbol.data = value;
			}

			// Expected format: /[DR]ef[0-9A-F]+/i
			if (token[0] == 'R' || token[0] == 'r') {
				symbol.type = SYMTYPE_IMPORT;
				// TODO: hard error if the rest is not zero
			} else if (token[0] != 'D' && token[0] != 'd') {
				fatal(&where, lineNo, "'S' line is neither \"Def\" nor \"Ref\"");
			} else {
				// All symbols are exported
				symbol.type = SYMTYPE_EXPORT;
				Symbol const *other = sym_GetSymbol(symbol.name);

				if (other) {
					// The same symbol can only be defined twice if neither
					// definition is in a floating section
					auto checkSymbol = [](Symbol const &sym) -> std::tuple<Section *, int32_t> {
						if (auto *label = std::get_if<Label>(&sym.data); label)
							return {label->section, label->offset};
						assume(std::holds_alternative<int32_t>(sym.data));
						return {nullptr, std::get<int32_t>(sym.data)};
					};
					auto [symbolSection, symbolValue] = checkSymbol(symbol);
					auto [otherSection, otherValue] = checkSymbol(*other);

					if ((otherSection && !otherSection->isAddressFixed)
					    || (symbolSection && !symbolSection->isAddressFixed)) {
						sym_AddSymbol(symbol); // This will error out
					} else if (otherValue != symbolValue) {
						error(
						    &where,
						    lineNo,
						    "Definition of \"%s\" conflicts with definition in %s (%" PRId32
						    " != %" PRId32 ")",
						    symbol.name.c_str(),
						    other->objFileName,
						    symbolValue,
						    otherValue
						);
					}
				} else {
					// Add a new definition
					sym_AddSymbol(symbol);
				}
				// It's fine to keep modifying the symbol after `AddSymbol`, only
				// the name must not be modified
			}
			if (strncasecmp(&token[1], "ef", 2) != 0)
				fatal(&where, lineNo, "'S' line is neither \"Def\" nor \"Ref\"");

			if (!fileSections.empty())
				fileSections.back().section->symbols.push_back(&symbol);

			expectEol("'S' line is too long");
			break;
		}

		case 'T':
			// Now, time to parse the data!
			if (!data.empty())
				warning(&where, lineNo, "Previous 'T' line had no 'R' line (ignored)");

			data.clear();
			for (token = strtok(line.data(), delim); token; token = strtok(nullptr, delim))
				data.push_back(parseByte(where, lineNo, token, numberType));

			if (data.size() < ADDR_SIZE)
				fatal(&where, lineNo, "'T' line is too short");
			// Importantly, now we know that there is "pending data" in `data`
			break;

		case 'R': {
			// Supposed to directly follow `T`
			if (data.empty()) {
				warning(&where, lineNo, "'R' line with no 'T' line, ignoring");
				break;
			}

			// First two bytes are ignored
			getToken(line.data(), "'R' line is too short");
			getToken(nullptr, "'R' line is too short");
			uint16_t areaIdx;

			getToken(nullptr, "'R' line is too short");
			areaIdx = parseByte(where, lineNo, token, numberType);
			getToken(nullptr, "'R' line is too short");
			areaIdx |= (uint16_t)parseByte(where, lineNo, token, numberType) << 8;
			if (areaIdx >= fileSections.size())
				fatal(
				    &where,
				    lineNo,
				    "'R' line references area #%" PRIu16 ", but there are only %zu (so far)",
				    areaIdx,
				    fileSections.size()
				);
			assume(!fileSections.empty()); // There should be at least one, from the above check
			Section *section = fileSections[areaIdx].section.get();
			uint16_t *writeIndex = &fileSections[areaIdx].writeIndex;
			uint8_t writtenOfs = ADDR_SIZE; // Bytes before this have been written to `->data`
			uint16_t addr = data[0] | data[1] << 8;

			if (section->isAddressFixed) {
				if (addr < section->org)
					fatal(
					    &where,
					    lineNo,
					    "'T' line reports address $%04" PRIx16
					    " in \"%s\", which starts at $%04" PRIx16,
					    addr,
					    section->name.c_str(),
					    section->org
					);
				addr -= section->org;
			}
			// Lines are emitted that violate this check but contain no "payload";
			// ignore those. "Empty" lines shouldn't trigger allocation, either.
			if (data.size() != ADDR_SIZE) {
				if (addr != *writeIndex)
					fatal(
					    &where,
					    lineNo,
					    "'T' lines which don't append to their section are not supported (%" PRIu16
					    " != %" PRIu16 ")",
					    addr,
					    *writeIndex
					);
				if (section->data.empty()) {
					assume(section->size != 0);
					section->data.resize(section->size);
				}
			}

			// Processing relocations is made difficult by SDLD's honestly quite bonkers
			// handling of the thing.
			// The way they work is that 16-bit relocs are, simply enough, writing a
			// 16-bit value over a 16-bit "gap". Nothing weird here.
			// 8-bit relocs, however, do not write an 8-bit value over an 8-bit gap!
			// They write an 8-bit value over a 16-bit gap... and either of the two
			// bytes is *discarded*. The "24-bit" flag extends this behavior to three
			// bytes instead of two, but the idea's the same.
			// Additionally, the "offset" is relative to *before* bytes from previous
			// relocs are removed, so this needs to be accounted for as well.
			// This all can be "translated" to RGBDS parlance by generating the
			// appropriate RPN expression (depending on flags), plus an addition for the
			// bytes being patched over.
			while ((token = strtok(nullptr, delim)) != nullptr) {
				uint16_t flags = parseByte(where, lineNo, token, numberType);

				if ((flags & 0xF0) == 0xF0) {
					getToken(nullptr, "Incomplete relocation");
					flags =
					    (flags & 0x0F) | (uint16_t)parseByte(where, lineNo, token, numberType) << 4;
				}

				getToken(nullptr, "Incomplete relocation");
				uint8_t offset = parseByte(where, lineNo, token, numberType);

				if (offset < ADDR_SIZE)
					fatal(
					    &where,
					    lineNo,
					    "Relocation index cannot point to header (%" PRIu16 " < %u)",
					    offset,
					    ADDR_SIZE
					);
				if (offset >= data.size())
					fatal(
					    &where,
					    lineNo,
					    "Relocation index is out of bounds (%" PRIu16 " >= %zu)",
					    offset,
					    data.size()
					);

				getToken(nullptr, "Incomplete relocation");
				uint16_t idx = parseByte(where, lineNo, token, numberType);

				getToken(nullptr, "Incomplete relocation");
				idx |= (uint16_t)parseByte(where, lineNo, token, numberType);

				// Loudly fail on unknown flags
				if (flags & (1 << RELOC_ZPAGE | 1 << RELOC_NPAGE))
					fatal(&where, lineNo, "Paging flags are not supported");
				if (flags & ~RELOC_ALL_FLAGS)
					warning(&where, lineNo, "Unknown reloc flags 0x%x", flags & ~RELOC_ALL_FLAGS);

				// Turn this into a Patch
				Patch &patch = section->patches.emplace_back();

				patch.lineNo = lineNo;
				patch.src = &where;
				patch.offset = offset - writtenOfs + *writeIndex;
				if (section->patches.size() > 1) {
					uint32_t prevOffset = section->patches[section->patches.size() - 2].offset;
					if (prevOffset >= patch.offset)
						fatal(
						    &where,
						    lineNo,
						    "Relocs not sorted by offset are not supported (%" PRIu32 " >= %" PRIu32
						    ")",
						    prevOffset,
						    patch.offset
						);
				}
				patch.pcSection = section;         // No need to fill `pcSectionID`, then
				patch.pcOffset = patch.offset - 1; // For `jr`s

				patch.type = (flags & 1 << RELOC_SIZE) ? PATCHTYPE_BYTE : PATCHTYPE_WORD;
				uint8_t nbBaseBytes = patch.type == PATCHTYPE_BYTE ? ADDR_SIZE : 2;
				uint32_t baseValue = 0;

				assume(offset < data.size());
				if (data.size() - offset < nbBaseBytes)
					fatal(
					    &where,
					    lineNo,
					    "Reloc would patch out of bounds (%" PRIu8 " > %zu)",
					    nbBaseBytes,
					    data.size() - offset
					);
				for (uint8_t i = 0; i < nbBaseBytes; ++i)
					baseValue = baseValue | data[offset + i] << (8 * i);

				// Bit 4 specifies signedness, but I don't think that matters?
				// Generate a RPN expression from the info and flags
				if (flags & 1 << RELOC_ISSYM) {
					if (idx >= fileSymbols.size())
						fatal(
						    &where,
						    lineNo,
						    "Reloc refers to symbol #%" PRIu16 " out of %zu",
						    idx,
						    fileSymbols.size()
						);
					Symbol const &sym = fileSymbols[idx];

					// SDCC has a bunch of "magic symbols" that start with a
					// letter and an underscore. These are not compatibility
					// hacks, this is how SDLD actually works.
					if (sym.name.starts_with("b_")) {
						// Look for the symbol being referenced, and use its index instead
						for (idx = 0; idx < fileSymbols.size(); ++idx) {
							if (sym.name.ends_with(fileSymbols[idx].name)
							    && 1 + sym.name.length() == fileSymbols[idx].name.length())
								break;
						}
						if (idx == fileSymbols.size())
							fatal(
							    &where,
							    lineNo,
							    "\"%s\" is missing a reference to \"%s\"",
							    sym.name.c_str(),
							    &sym.name.c_str()[1]
							);
						patch.rpnExpression.resize(5);
						patch.rpnExpression[0] = RPN_BANK_SYM;
						patch.rpnExpression[1] = idx;
						patch.rpnExpression[2] = idx >> 8;
						patch.rpnExpression[3] = idx >> 16;
						patch.rpnExpression[4] = idx >> 24;
					} else if (sym.name.starts_with("l_")) {
						patch.rpnExpression.resize(1 + sym.name.length() - 2 + 1);
						patch.rpnExpression[0] = RPN_SIZEOF_SECT;
						memcpy(
						    (char *)&patch.rpnExpression[1],
						    &sym.name.c_str()[2],
						    sym.name.length() - 2 + 1
						);
					} else if (sym.name.starts_with("s_")) {
						patch.rpnExpression.resize(1 + sym.name.length() - 2 + 1);
						patch.rpnExpression[0] = RPN_STARTOF_SECT;
						memcpy(
						    (char *)&patch.rpnExpression[1],
						    &sym.name.c_str()[2],
						    sym.name.length() - 2 + 1
						);
					} else {
						patch.rpnExpression.resize(5);
						patch.rpnExpression[0] = RPN_SYM;
						patch.rpnExpression[1] = idx;
						patch.rpnExpression[2] = idx >> 8;
						patch.rpnExpression[3] = idx >> 16;
						patch.rpnExpression[4] = idx >> 24;
					}
				} else {
					if (idx >= fileSections.size())
						fatal(
						    &where,
						    lineNo,
						    "Reloc refers to area #%" PRIu16 " out of %zu",
						    idx,
						    fileSections.size()
						);
					// It gets funky. If the area is absolute, *actually*, we
					// must not add its base address, as the assembler will
					// already have added it in `baseValue`.
					// We counteract this by subtracting the section's base
					// address from `baseValue`, undoing what the assembler did;
					// this allows the relocation to still be correct, even if
					// the section gets moved for any reason.
					if (fileSections[idx].section->isAddressFixed)
						baseValue -= fileSections[idx].section->org;
					std::string const &name = fileSections[idx].section->name;
					Section const *other = sect_GetSection(name);

					// Unlike with `s_<AREA>`, referencing an area in this way
					// wants the beginning of this fragment, so we must add the
					// fragment's (putative) offset to account for this.
					// The fragment offset prediction is guaranteed since each
					// section can only have one fragment per SDLD object file,
					// so this fragment will be appended to the existing section
					// *if any*, and thus its offset will be the section's
					// current size.
					if (other)
						baseValue += other->size;
					patch.rpnExpression.resize(1 + name.length() + 1);
					patch.rpnExpression[0] = RPN_STARTOF_SECT;
					// The cast is fine, it's just different signedness
					memcpy((char *)&patch.rpnExpression[1], name.c_str(), name.length() + 1);
				}

				patch.rpnExpression.push_back(RPN_CONST);
				patch.rpnExpression.push_back(baseValue);
				patch.rpnExpression.push_back(baseValue >> 8);
				patch.rpnExpression.push_back(baseValue >> 16);
				patch.rpnExpression.push_back(baseValue >> 24);
				patch.rpnExpression.push_back(RPN_ADD);

				if (patch.type == PATCHTYPE_BYTE) {
					// Despite the flag's name, as soon as it is set, 3 bytes
					// are present, so we must skip two of them
					if (flags & 1 << RELOC_EXPR16) {
						if (*writeIndex + (offset - writtenOfs) > section->size)
							fatal(
							    &where,
							    lineNo,
							    "'T' line writes past \"%s\"'s end (%u > %" PRIu16 ")",
							    section->name.c_str(),
							    *writeIndex + (offset - writtenOfs),
							    section->size
							);
						// Copy all bytes up to those (plus the byte that we'll overwrite)
						memcpy(
						    &section->data[*writeIndex], &data[writtenOfs], offset - writtenOfs + 1
						);
						*writeIndex += offset - writtenOfs + 1;
						writtenOfs = offset + 3; // Skip all three `baseValue` bytes, though
					}

					// Append the necessary operations...
					if (flags & 1 << RELOC_ISPCREL) {
						// The result must *not* be truncated for those!
						patch.type = PATCHTYPE_JR;
						// TODO: check the other flags?
					} else if (flags & 1 << RELOC_EXPR24 && flags & 1 << RELOC_BANKBYTE) {
						patch.rpnExpression.push_back(RPN_CONST);
						patch.rpnExpression.push_back(16);
						patch.rpnExpression.push_back(16 >> 8);
						patch.rpnExpression.push_back(16 >> 16);
						patch.rpnExpression.push_back(16 >> 24);
						patch.rpnExpression.push_back(
						    (flags & 1 << RELOC_SIGNED) ? RPN_SHR : RPN_USHR
						);
					} else {
						if (flags & 1 << RELOC_EXPR16 && flags & 1 << RELOC_WHICHBYTE) {
							patch.rpnExpression.push_back(RPN_CONST);
							patch.rpnExpression.push_back(8);
							patch.rpnExpression.push_back(8 >> 8);
							patch.rpnExpression.push_back(8 >> 16);
							patch.rpnExpression.push_back(8 >> 24);
							patch.rpnExpression.push_back(
							    (flags & 1 << RELOC_SIGNED) ? RPN_SHR : RPN_USHR
							);
						}
						patch.rpnExpression.push_back(RPN_CONST);
						patch.rpnExpression.push_back(0xFF);
						patch.rpnExpression.push_back(0xFF >> 8);
						patch.rpnExpression.push_back(0xFF >> 16);
						patch.rpnExpression.push_back(0xFF >> 24);
						patch.rpnExpression.push_back(RPN_AND);
					}
				} else if (flags & 1 << RELOC_ISPCREL) {
					assume(patch.type == PATCHTYPE_WORD);
					fatal(&where, lineNo, "16-bit PC-relative relocations are not supported");
				} else if (flags & (1 << RELOC_EXPR16 | 1 << RELOC_EXPR24)) {
					fatal(
					    &where,
					    lineNo,
					    "Flags 0x%x are not supported for 16-bit relocs",
					    flags & (1 << RELOC_EXPR16 | 1 << RELOC_EXPR24)
					);
				}
			}

			// If there is some data left to append, do so
			if (writtenOfs != data.size()) {
				assume(data.size() > writtenOfs);
				if (*writeIndex + (data.size() - writtenOfs) > section->size)
					fatal(
					    &where,
					    lineNo,
					    "'T' line writes past \"%s\"'s end (%zu > %" PRIu16 ")",
					    section->name.c_str(),
					    *writeIndex + (data.size() - writtenOfs),
					    section->size
					);
				memcpy(&section->data[*writeIndex], &data[writtenOfs], data.size() - writtenOfs);
				*writeIndex += data.size() - writtenOfs;
			}

			data.clear(); // Do not allow two R lines to refer to the same T line
			break;
		}

		case 'P':
		default:
			warning(&where, lineNo, "Unknown/unsupported line type '%c', ignoring", lineType);
			break;
		}
	}

#undef expectEol
#undef expectToken
#undef getToken

	if (!data.empty())
		warning(&where, lineNo, "Last 'T' line had no 'R' line (ignored)");
	if (fileSections.size() < expectedNbAreas)
		warning(
		    &where,
		    lineNo,
		    "Expected %" PRIu32 " 'A' lines, got only %zu",
		    expectedNbAreas,
		    fileSections.size()
		);
	if (fileSymbols.size() < expectedNbSymbols)
		warning(
		    &where,
		    lineNo,
		    "Expected %" PRIu32 " 'S' lines, got only %zu",
		    expectedNbSymbols,
		    fileSymbols.size()
		);

	nbSectionsToAssign += fileSections.size();

	for (FileSection &entry : fileSections) {
		std::unique_ptr<Section> &section = entry.section;

		// RAM sections can have a size, but don't get any data (they shouldn't have any)
		if (entry.writeIndex != section->size && entry.writeIndex != 0)
			fatal(
			    &where,
			    lineNo,
			    "\"%s\" was not fully written (%" PRIu16 " < %" PRIu16 ")",
			    section->name.c_str(),
			    entry.writeIndex,
			    section->size
			);

		if (section->modifier == SECTION_FRAGMENT) {
			// Add the fragment's offset to all of its symbols
			for (Symbol *symbol : section->symbols)
				symbol->label().offset += section->offset;
		}

		// Calling `sect_AddSection` invalidates the contents of `fileSections`!
		sect_AddSection(std::move(section));
	}
}
