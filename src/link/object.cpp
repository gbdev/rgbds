// SPDX-License-Identifier: MIT

#include "link/object.hpp"

#include <deque>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "diagnostics.hpp"
#include "helpers.hpp"
#include "linkdefs.hpp"
#include "platform.hpp"
#include "version.hpp"

#include "link/assign.hpp"
#include "link/main.hpp"
#include "link/patch.hpp"
#include "link/sdas_obj.hpp"
#include "link/section.hpp"
#include "link/symbol.hpp"
#include "link/warning.hpp"

static std::deque<std::vector<Symbol>> symbolLists;
static std::vector<std::vector<FileStackNode>> nodes;

// Helper functions for reading object files

// For internal use only by `tryReadLong` and `tryGetc`!
#define tryRead(func, type, errval, vartype, var, file, ...) \
	do { \
		FILE *tmpFile = file; \
		type tmpVal = func(tmpFile); \
		if (tmpVal == (errval)) { \
			fatal(__VA_ARGS__, feof(tmpFile) ? "Unexpected end of file" : strerror(errno)); \
		} \
		var = static_cast<vartype>(tmpVal); \
	} while (0)

// Reads an unsigned long (32-bit) value from a file, or `INT64_MAX` on failure.
static int64_t readLong(FILE *file) {
	uint32_t value = 0;

	// Read the little-endian value byte by byte
	for (uint8_t shift = 0; shift < sizeof(value) * CHAR_BIT; shift += 8) {
		int byte = getc(file);

		if (byte == EOF) {
			return INT64_MAX;
		}
		// This must be casted to `unsigned`, not `uint8_t`. Rationale:
		// the type of the shift is the type of `byte` after undergoing
		// integer promotion, which would be `int` if this was casted to
		// `uint8_t`, because int is large enough to hold a byte. This
		// however causes values larger than 127 to be too large when
		// shifted, potentially triggering undefined behavior.
		value |= static_cast<unsigned int>(byte) << shift;
	}
	return value;
}

// Helper macro to read a long from a file to a var, or error out if it fails to.
#define tryReadLong(var, file, ...) \
	tryRead(readLong, int64_t, INT64_MAX, long, var, file, __VA_ARGS__)

// Helper macro to read a byte from a file to a var, or error out if it fails to.
#define tryGetc(type, var, file, ...) tryRead(getc, int, EOF, type, var, file, __VA_ARGS__)

// Helper macro to read a '\0'-terminated string from a file, or error out if it fails to.
#define tryReadString(var, file, ...) \
	do { \
		FILE *tmpFile = file; \
		std::string &tmpVal = var; \
		for (int tmpByte; (tmpByte = getc(tmpFile)) != '\0';) { \
			if (tmpByte == EOF) { \
				fatal(__VA_ARGS__, feof(tmpFile) ? "Unexpected end of file" : strerror(errno)); \
			} else { \
				tmpVal.push_back(tmpByte); \
			} \
		} \
	} while (0)

// Functions to parse object files

// Reads a file stack node from a file.
static void readFileStackNode(
    FILE *file, std::vector<FileStackNode> &fileNodes, uint32_t nodeID, char const *fileName
) {
	FileStackNode &node = fileNodes[nodeID];

	uint32_t parentID;
	tryReadLong(
	    parentID, file, "%s: Cannot read node #%" PRIu32 "'s parent ID: %s", fileName, nodeID
	);
	node.parent = parentID != UINT32_MAX ? &fileNodes[parentID] : nullptr;

	tryReadLong(
	    node.lineNo, file, "%s: Cannot read node #%" PRIu32 "'s line number: %s", fileName, nodeID
	);
	tryGetc(
	    FileStackNodeType,
	    node.type,
	    file,
	    "%s: Cannot read node #%" PRIu32 "'s type: %s",
	    fileName,
	    nodeID
	);
	switch (node.type) {
	case NODE_FILE:
	case NODE_MACRO:
		node.data = "";
		tryReadString(
		    node.name(), file, "%s: Cannot read node #%" PRIu32 "'s file name: %s", fileName, nodeID
		);
		break;

		uint32_t depth;
	case NODE_REPT:
		tryReadLong(
		    depth, file, "%s: Cannot read node #%" PRIu32 "'s rept depth: %s", fileName, nodeID
		);
		node.data = std::vector<uint32_t>(depth);
		for (uint32_t i = 0; i < depth; ++i) {
			tryReadLong(
			    node.iters()[i],
			    file,
			    "%s: Cannot read node #%" PRIu32 "'s iter #%" PRIu32 ": %s",
			    fileName,
			    nodeID,
			    i
			);
		}
		if (!node.parent) {
			fatal(
			    "%s is not a valid object file: root node (#%" PRIu32 ") may not be REPT",
			    fileName,
			    nodeID
			);
		}
	}
}

// Reads a symbol from a file.
static void readSymbol(
    FILE *file, Symbol &symbol, char const *fileName, std::vector<FileStackNode> const &fileNodes
) {
	tryReadString(symbol.name, file, "%s: Cannot read symbol name: %s", fileName);
	tryGetc(
	    ExportLevel,
	    symbol.type,
	    file,
	    "%s: Cannot read \"%s\"'s type: %s",
	    fileName,
	    symbol.name.c_str()
	);
	// If the symbol is defined in this file, read its definition
	if (symbol.type != SYMTYPE_IMPORT) {
		uint32_t nodeID;
		tryReadLong(
		    nodeID, file, "%s: Cannot read \"%s\"'s node ID: %s", fileName, symbol.name.c_str()
		);
		symbol.src = &fileNodes[nodeID];
		tryReadLong(
		    symbol.lineNo,
		    file,
		    "%s: Cannot read \"%s\"'s line number: %s",
		    fileName,
		    symbol.name.c_str()
		);
		int32_t sectionID, value;
		tryReadLong(
		    sectionID,
		    file,
		    "%s: Cannot read \"%s\"'s section ID: %s",
		    fileName,
		    symbol.name.c_str()
		);
		tryReadLong(
		    value, file, "%s: Cannot read \"%s\"'s value: %s", fileName, symbol.name.c_str()
		);
		if (sectionID == -1) {
			symbol.data = value;
		} else {
			symbol.data = Label{
			    .sectionID = sectionID,
			    .offset = value,
			    // Set the `.section` later based on the `.sectionID`
			    .section = nullptr,
			};
		}
	} else {
		symbol.data = -1;
	}
}

// Reads a patch from a file.
static void readPatch(
    FILE *file,
    Patch &patch,
    char const *fileName,
    std::string const &sectName,
    uint32_t patchID,
    std::vector<FileStackNode> const &fileNodes
) {
	uint32_t nodeID;
	tryReadLong(
	    nodeID,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s node ID: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);
	patch.src = &fileNodes[nodeID];

	tryReadLong(
	    patch.lineNo,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s line number: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);
	tryReadLong(
	    patch.offset,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s offset: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);
	tryReadLong(
	    patch.pcSectionID,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s PC offset: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);
	tryReadLong(
	    patch.pcOffset,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s PC offset: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);

	PatchType type;
	tryGetc(
	    PatchType,
	    type,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s type: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);
	patch.type = type;

	uint32_t rpnSize;
	tryReadLong(
	    rpnSize,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s RPN size: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);

	patch.rpnExpression.resize(rpnSize);
	if (fread(patch.rpnExpression.data(), 1, rpnSize, file) != rpnSize) {
		fatal(
		    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s RPN expression: %s",
		    fileName,
		    sectName.c_str(),
		    patchID,
		    feof(file) ? "Unexpected end of file" : strerror(errno)
		);
	}
}

// Sets a patch's `pcSection` from its `pcSectionID`.
static void
    linkPatchToPCSect(Patch &patch, std::vector<std::unique_ptr<Section>> const &fileSections) {
	patch.pcSection =
	    patch.pcSectionID != UINT32_MAX ? fileSections[patch.pcSectionID].get() : nullptr;
}

// Reads a section from a file.
static void readSection(
    FILE *file, Section &section, char const *fileName, std::vector<FileStackNode> const &fileNodes
) {
	int32_t tmp;
	uint8_t byte;

	tryReadString(section.name, file, "%s: Cannot read section name: %s", fileName);
	uint32_t nodeID;
	tryReadLong(
	    nodeID, file, "%s: Cannot read \"%s\"'s node ID: %s", fileName, section.name.c_str()
	);
	section.src = &fileNodes[nodeID];
	tryReadLong(
	    section.lineNo,
	    file,
	    "%s: Cannot read \"%s\"'s line number: %s",
	    fileName,
	    section.name.c_str()
	);
	tryReadLong(tmp, file, "%s: Cannot read \"%s\"'s' size: %s", fileName, section.name.c_str());
	if (tmp < 0 || tmp > UINT16_MAX) {
		fatal("\"%s\"'s section size ($%" PRIx32 ") is invalid", section.name.c_str(), tmp);
	}
	section.size = tmp;
	section.offset = 0;
	tryGetc(
	    uint8_t, byte, file, "%s: Cannot read \"%s\"'s type: %s", fileName, section.name.c_str()
	);
	if (uint8_t type = byte & 0x3F; type >= SECTTYPE_INVALID) {
		fatal("\"%s\" has unknown section type 0x%02x", section.name.c_str(), type);
	} else {
		section.type = SectionType(type);
	}
	if (byte >> 7) {
		section.modifier = SECTION_UNION;
	} else if (byte >> 6) {
		section.modifier = SECTION_FRAGMENT;
	} else {
		section.modifier = SECTION_NORMAL;
	}
	tryReadLong(tmp, file, "%s: Cannot read \"%s\"'s org: %s", fileName, section.name.c_str());
	section.isAddressFixed = tmp >= 0;
	if (tmp > UINT16_MAX) {
		error("\"%s\"'s org is too large ($%" PRIx32 ")", section.name.c_str(), tmp);
		tmp = UINT16_MAX;
	}
	section.org = tmp;
	tryReadLong(tmp, file, "%s: Cannot read \"%s\"'s bank: %s", fileName, section.name.c_str());
	section.isBankFixed = tmp >= 0;
	section.bank = tmp;
	tryGetc(
	    uint8_t,
	    byte,
	    file,
	    "%s: Cannot read \"%s\"'s alignment: %s",
	    fileName,
	    section.name.c_str()
	);
	if (byte > 16) {
		byte = 16;
	}
	section.isAlignFixed = byte != 0;
	section.alignMask = (1 << byte) - 1;
	tryReadLong(
	    tmp, file, "%s: Cannot read \"%s\"'s alignment offset: %s", fileName, section.name.c_str()
	);
	if (tmp > UINT16_MAX) {
		error("\"%s\"'s alignment offset is too large ($%" PRIx32 ")", section.name.c_str(), tmp);
		tmp = UINT16_MAX;
	}
	section.alignOfs = tmp;

	if (sect_HasData(section.type)) {
		if (section.size) {
			section.data.resize(section.size);
			if (fread(section.data.data(), 1, section.size, file) != section.size) {
				fatal(
				    "%s: Cannot read \"%s\"'s data: %s",
				    fileName,
				    section.name.c_str(),
				    feof(file) ? "Unexpected end of file" : strerror(errno)
				);
			}
		}

		uint32_t nbPatches;
		tryReadLong(
		    nbPatches,
		    file,
		    "%s: Cannot read \"%s\"'s number of patches: %s",
		    fileName,
		    section.name.c_str()
		);

		section.patches.resize(nbPatches);
		for (uint32_t i = 0; i < nbPatches; ++i) {
			readPatch(file, section.patches[i], fileName, section.name, i, fileNodes);
		}
	}
}

// Links a symbol to a section, keeping the section's symbol list sorted.
static void linkSymToSect(Symbol &symbol, Section &section) {
	uint32_t a = 0, b = section.symbols.size();
	int32_t symbolOffset = symbol.label().offset;

	while (a != b) {
		uint32_t c = (a + b) / 2;
		int32_t otherOffset = section.symbols[c]->label().offset;

		if (otherOffset > symbolOffset) {
			b = c;
		} else {
			a = c + 1;
		}
	}

	section.symbols.insert(section.symbols.begin() + a, &symbol);
}

// Reads an assertion from a file.
static void readAssertion(
    FILE *file,
    Assertion &assert,
    char const *fileName,
    uint32_t assertID,
    std::vector<FileStackNode> const &fileNodes
) {
	std::string assertName("Assertion #");

	assertName += std::to_string(assertID);
	readPatch(file, assert.patch, fileName, assertName, 0, fileNodes);
	tryReadString(assert.message, file, "%s: Cannot read assertion's message: %s", fileName);
}

void obj_ReadFile(char const *fileName, unsigned int fileID) {
	FILE *file;
	if (strcmp(fileName, "-")) {
		file = fopen(fileName, "rb");
	} else {
		fileName = "<stdin>";
		(void)setmode(STDIN_FILENO, O_BINARY);
		file = stdin;
	}
	if (!file) {
		fatal("Failed to open file \"%s\": %s", fileName, strerror(errno));
	}
	Defer closeFile{[&] { fclose(file); }};

	// First, check if the object is a RGBDS object or a SDCC one. If the first byte is 'R',
	// we'll assume it's a RGBDS object file, and otherwise, that it's a SDCC object file.
	int c = getc(file);

	ungetc(c, file); // Guaranteed to work
	switch (c) {
	case EOF:
		fatal("File \"%s\" is empty!", fileName);

	case 'R':
		break;

	default:
		// This is (probably) a SDCC object file, defer the rest of detection to it.
		// Since SDCC does not provide line info, everything will be reported as coming from the
		// object file. It's better than nothing.
		nodes[fileID].push_back({
		    .type = NODE_FILE,
		    .data = std::variant<std::monostate, std::vector<uint32_t>, std::string>(fileName),
		    .parent = nullptr,
		    .lineNo = 0,
		});

		std::vector<Symbol> &fileSymbols = symbolLists.emplace_front();

		sdobj_ReadFile(nodes[fileID].back(), file, fileSymbols);
		return;
	}

	// Begin by reading the magic bytes
	int matchedElems;

	if (fscanf(file, RGBDS_OBJECT_VERSION_STRING "%n", &matchedElems) == 1
	    && matchedElems != literal_strlen(RGBDS_OBJECT_VERSION_STRING)) {
		fatal("%s: Not a RGBDS object file", fileName);
	}

	verbosePrint("Reading object file %s\n", fileName);

	uint32_t revNum;
	tryReadLong(revNum, file, "%s: Cannot read revision number: %s", fileName);
	if (revNum != RGBDS_OBJECT_REV) {
		fatal(
		    "%s: Unsupported object file for rgblink %s; try rebuilding \"%s\"%s"
		    " (expected revision %d, got %d)",
		    fileName,
		    get_package_version_string(),
		    fileName,
		    revNum > RGBDS_OBJECT_REV ? " or updating rgblink" : "",
		    RGBDS_OBJECT_REV,
		    revNum
		);
	}

	uint32_t nbSymbols;
	tryReadLong(nbSymbols, file, "%s: Cannot read number of symbols: %s", fileName);

	uint32_t nbSections;
	tryReadLong(nbSections, file, "%s: Cannot read number of sections: %s", fileName);

	uint32_t nbNodes;
	tryReadLong(nbNodes, file, "%s: Cannot read number of nodes: %s", fileName);
	nodes[fileID].resize(nbNodes);
	verbosePrint("Reading %u nodes...\n", nbNodes);
	for (uint32_t i = nbNodes; i--;) {
		readFileStackNode(file, nodes[fileID], i, fileName);
	}

	// This file's symbols, kept to link sections to them
	std::vector<Symbol> &fileSymbols = symbolLists.emplace_front(nbSymbols);
	std::vector<uint32_t> nbSymPerSect(nbSections, 0);

	verbosePrint("Reading %" PRIu32 " symbols...\n", nbSymbols);
	for (uint32_t i = 0; i < nbSymbols; ++i) {
		// Read symbol
		Symbol &symbol = fileSymbols[i];

		readSymbol(file, symbol, fileName, nodes[fileID]);

		sym_AddSymbol(symbol);
		if (std::holds_alternative<Label>(symbol.data)) {
			++nbSymPerSect[std::get<Label>(symbol.data).sectionID];
		}
	}

	// This file's sections, stored in a table to link symbols to them
	std::vector<std::unique_ptr<Section>> fileSections(nbSections);

	verbosePrint("Reading %" PRIu32 " sections...\n", nbSections);
	for (uint32_t i = 0; i < nbSections; ++i) {
		// Read section
		fileSections[i] = std::make_unique<Section>();
		fileSections[i]->nextu = nullptr;
		readSection(file, *fileSections[i], fileName, nodes[fileID]);
		fileSections[i]->fileSymbols = &fileSymbols;
		fileSections[i]->symbols.reserve(nbSymPerSect[i]);
	}

	uint32_t nbAsserts;
	tryReadLong(nbAsserts, file, "%s: Cannot read number of assertions: %s", fileName);
	verbosePrint("Reading %" PRIu32 " assertions...\n", nbAsserts);
	for (uint32_t i = 0; i < nbAsserts; ++i) {
		Assertion &assertion = patch_AddAssertion();

		readAssertion(file, assertion, fileName, i, nodes[fileID]);
		linkPatchToPCSect(assertion.patch, fileSections);
		assertion.fileSymbols = &fileSymbols;
	}

	// Give patches' PC section pointers to their sections
	for (uint32_t i = 0; i < nbSections; ++i) {
		if (sect_HasData(fileSections[i]->type)) {
			for (Patch &patch : fileSections[i]->patches) {
				linkPatchToPCSect(patch, fileSections);
			}
		}
	}

	// Give symbols' section pointers to their sections
	for (uint32_t i = 0; i < nbSymbols; ++i) {
		if (std::holds_alternative<Label>(fileSymbols[i].data)) {
			Label &label = std::get<Label>(fileSymbols[i].data);
			label.section = fileSections[label.sectionID].get();
			// Give the section a pointer to the symbol as well
			linkSymToSect(fileSymbols[i], *label.section);
		}
	}

	// Calling `sect_AddSection` invalidates the contents of `fileSections`!
	for (uint32_t i = 0; i < nbSections; ++i) {
		sect_AddSection(std::move(fileSections[i]));
	}

	// Fix symbols' section pointers to component sections
	// This has to run **after** all the `sect_AddSection()` calls,
	// so that `sect_GetSection()` will work
	for (uint32_t i = 0; i < nbSymbols; ++i) {
		if (std::holds_alternative<Label>(fileSymbols[i].data)) {
			Label &label = std::get<Label>(fileSymbols[i].data);
			Section *section = label.section;
			if (section->modifier != SECTION_NORMAL) {
				// Associate the symbol with the main section, not the "component" one
				label.section = sect_GetSection(section->name);
			}
			if (section->modifier == SECTION_FRAGMENT) {
				// Add the fragment's offset to the symbol's
				// (`section->offset` is computed by `sect_AddSection`)
				label.offset += section->offset;
			}
		}
	}
}

void obj_Setup(unsigned int nbFiles) {
	nodes.resize(nbFiles);
}
