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
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "helpers.hpp"
#include "linkdefs.hpp"
#include "platform.hpp"
#include "verbosity.hpp"
#include "version.hpp"

#include "link/fstack.hpp"
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
#define tryGetc(var, file, ...) tryRead(getc, int, EOF, uint8_t, var, file, __VA_ARGS__)

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
	if (parentID == UINT32_MAX) {
		node.parent = nullptr;
	} else if (parentID >= fileNodes.size()) {
		fatal("%s: Node #%" PRIu32 " has invalid parent ID #%" PRIu32, fileName, nodeID, parentID);
	} else {
		node.parent = &fileNodes[parentID];
	}

	tryReadLong(
	    node.lineNo, file, "%s: Cannot read node #%" PRIu32 "'s line number: %s", fileName, nodeID
	);

	uint8_t type;
	tryGetc(type, file, "%s: Cannot read node #%" PRIu32 "'s type: %s", fileName, nodeID);
	switch (type & ~(1 << FSTACKNODE_QUIET_BIT)) {
	case NODE_FILE:
	case NODE_MACRO:
		node.type = FileStackNodeType(type);
		node.data = "";
		tryReadString(
		    node.name(), file, "%s: Cannot read node #%" PRIu32 "'s file name: %s", fileName, nodeID
		);
		break;
	case NODE_REPT: {
		node.type = NODE_REPT;
		uint32_t depth;
		tryReadLong(
		    depth, file, "%s: Cannot read node #%" PRIu32 "'s REPT depth: %s", fileName, nodeID
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
			    "%s: Invalid object file: root node (#%" PRIu32 ") may not be REPT",
			    fileName,
			    nodeID
			);
		}
		break;
	}
	default:
		fatal("%s: Node #%" PRIu32 " has unknown type 0x%02x", fileName, nodeID, type);
	}

	node.isQuiet = (type & (1 << FSTACKNODE_QUIET_BIT)) != 0;
}

// Reads a symbol from a file.
static void readSymbol(
    FILE *file, Symbol &symbol, char const *fileName, std::vector<FileStackNode> const &fileNodes
) {
	tryReadString(symbol.name, file, "%s: Cannot read symbol name: %s", fileName);

	uint8_t type;
	tryGetc(type, file, "%s: Cannot read `%s`'s type: %s", fileName, symbol.name.c_str());
	if (type >= SYMTYPE_INVALID) {
		fatal("%s: `%s` has unknown type 0x%02x", fileName, symbol.name.c_str(), type);
	} else {
		symbol.type = ExportLevel(type);
	}

	// If the symbol is defined in this file, read its definition
	if (symbol.type != SYMTYPE_IMPORT) {
		uint32_t nodeID;
		tryReadLong(
		    nodeID, file, "%s: Cannot read `%s`'s node ID: %s", fileName, symbol.name.c_str()
		);
		if (nodeID >= fileNodes.size()) {
			fatal("%s: `%s` has invalid node ID #%" PRIu32, fileName, symbol.name.c_str(), nodeID);
		}

		symbol.src = &fileNodes[nodeID];
		tryReadLong(
		    symbol.lineNo,
		    file,
		    "%s: Cannot read `%s`'s line number: %s",
		    fileName,
		    symbol.name.c_str()
		);
		int32_t sectionID, value;
		tryReadLong(
		    sectionID, file, "%s: Cannot read `%s`'s section ID: %s", fileName, symbol.name.c_str()
		);
		tryReadLong(value, file, "%s: Cannot read `%s`'s value: %s", fileName, symbol.name.c_str());
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
	if (nodeID >= fileNodes.size()) {
		fatal(
		    "%s: \"%s\"'s patch #%" PRIu32 " has invalid node ID #%" PRIu32,
		    fileName,
		    sectName.c_str(),
		    patchID,
		    nodeID
		);
	}
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

	uint8_t type;
	tryGetc(
	    type,
	    file,
	    "%s: Cannot read \"%s\"'s patch #%" PRIu32 "'s type: %s",
	    fileName,
	    sectName.c_str(),
	    patchID
	);
	if (type >= PATCHTYPE_INVALID) {
		fatal(
		    "%s: \"%s\"'s patch #%" PRIu32 " has unknown type 0x%02x",
		    fileName,
		    sectName.c_str(),
		    patchID,
		    type
		);
	} else {
		patch.type = PatchType(type);
	}

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
	if (nodeID >= fileNodes.size()) {
		fatal("%s: \"%s\" has invalid node ID #%" PRIu32, fileName, section.name.c_str(), nodeID);
	}
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
		fatal(
		    "%s: \"%s\"'s section size ($%" PRIx32 ") is invalid",
		    fileName,
		    section.name.c_str(),
		    tmp
		);
	}
	section.size = tmp;
	section.offset = 0;

	tryGetc(byte, file, "%s: Cannot read \"%s\"'s type: %s", fileName, section.name.c_str());
	if (uint8_t type = byte & SECTTYPE_TYPE_MASK; type >= SECTTYPE_INVALID) {
		fatal("%s: \"%s\" has unknown section type 0x%02x", fileName, section.name.c_str(), type);
	} else {
		section.type = SectionType(type);
	}

	if (byte & (1 << SECTTYPE_UNION_BIT)) {
		section.modifier = SECTION_UNION;
	} else if (byte & (1 << SECTTYPE_FRAGMENT_BIT)) {
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
	tryGetc(byte, file, "%s: Cannot read \"%s\"'s alignment: %s", fileName, section.name.c_str());
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

	if (sectTypeHasData(section.type)) {
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

void obj_ReadFile(std::string const &filePath, size_t fileID) {
	FILE *file;
	char const *fileName = filePath.c_str();
	if (filePath != "-") {
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

	// First, check if the object is a RGBDS object, a SDCC one, or neither.
	// A single `ungetc` is guaranteed to work.
	switch (ungetc(getc(file), file)) {
	case EOF:
		fatal("File \"%s\" is empty", fileName);

	case 'X':
	case 'D':
	case 'Q': {
		// This is (probably) a SDCC object file, defer the rest of detection to it.
		// Since SDCC does not provide line info, everything will be reported as coming from the
		// object file. It's better than nothing.
		nodes[fileID].push_back({
		    .type = NODE_FILE,
		    .data = std::variant<std::monostate, std::vector<uint32_t>, std::string>(fileName),
		    .isQuiet = false,
		    .parent = nullptr,
		    .lineNo = 0,
		});

		std::vector<Symbol> &fileSymbols = symbolLists.emplace_front();

		sdobj_ReadFile(nodes[fileID].back(), file, fileSymbols);
		return;
	}

	case 'R':
		// Check the magic byte signature for a RGB object file.
		if (char magic[literal_strlen(RGBDS_OBJECT_VERSION_STRING)];
		    fread(magic, 1, sizeof(magic), file) == sizeof(magic)
		    && !memcmp(magic, RGBDS_OBJECT_VERSION_STRING, sizeof(magic))) {
			break;
		}
		[[fallthrough]];

	default:
		fatal("%s: Not a RGBDS object file", fileName);
	}

	verbosePrint(VERB_NOTICE, "Reading object file %s\n", fileName);

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
	verbosePrint(VERB_INFO, "Reading %u nodes...\n", nbNodes);
	for (uint32_t nodeID = nbNodes; nodeID--;) {
		readFileStackNode(file, nodes[fileID], nodeID, fileName);
	}

	// This file's symbols, kept to link sections to them
	std::vector<Symbol> &fileSymbols = symbolLists.emplace_front(nbSymbols);
	std::vector<uint32_t> nbSymPerSect(nbSections, 0);

	verbosePrint(VERB_INFO, "Reading %" PRIu32 " symbols...\n", nbSymbols);
	for (Symbol &sym : fileSymbols) {
		readSymbol(file, sym, fileName, nodes[fileID]);
		sym_AddSymbol(sym);
		if (std::holds_alternative<Label>(sym.data)) {
			int32_t sectionID = std::get<Label>(sym.data).sectionID;
			if (sectionID < 0 || static_cast<size_t>(sectionID) >= nbSymPerSect.size()) {
				fatal(
				    "%s: `%s` has invalid section ID #%" PRId32,
				    fileName,
				    sym.name.c_str(),
				    sectionID
				);
			}
			++nbSymPerSect[sectionID];
		}
	}

	// This file's sections, stored in a table to link symbols to them
	std::vector<std::unique_ptr<Section>> fileSections(nbSections);

	verbosePrint(VERB_INFO, "Reading %" PRIu32 " sections...\n", nbSections);
	for (uint32_t i = 0; i < nbSections; ++i) {
		fileSections[i] = std::make_unique<Section>();
		fileSections[i]->nextPiece = nullptr;
		readSection(file, *fileSections[i], fileName, nodes[fileID]);
		fileSections[i]->fileSymbols = &fileSymbols;
		fileSections[i]->symbols.reserve(nbSymPerSect[i]);
	}

	uint32_t nbAsserts;
	tryReadLong(nbAsserts, file, "%s: Cannot read number of assertions: %s", fileName);
	verbosePrint(VERB_INFO, "Reading %" PRIu32 " assertions...\n", nbAsserts);
	for (uint32_t i = 0; i < nbAsserts; ++i) {
		Assertion &assertion = patch_AddAssertion();

		readAssertion(file, assertion, fileName, i, nodes[fileID]);

		if (assertion.patch.pcSectionID == UINT32_MAX) {
			assertion.patch.pcSection = nullptr;
		} else if (assertion.patch.pcSectionID >= fileSections.size()) {
			fatal(
			    "%s: Assertion #%" PRIu32 "'s patch has invalid section ID #%" PRIu32,
			    fileName,
			    i,
			    assertion.patch.pcSectionID
			);
		} else {
			assertion.patch.pcSection = fileSections[assertion.patch.pcSectionID].get();
		}

		assertion.fileSymbols = &fileSymbols;
	}

	// Give patches' PC section pointers to their sections
	for (std::unique_ptr<Section> const &sect : fileSections) {
		if (!sectTypeHasData(sect->type)) {
			continue;
		}
		for (size_t i = 0; i < sect->patches.size(); ++i) {
			if (Patch &patch = sect->patches[i]; patch.pcSectionID == UINT32_MAX) {
				patch.pcSection = nullptr;
			} else if (patch.pcSectionID >= fileSections.size()) {
				fatal(
				    "%s: \"%s\"'s patch #%zu has invalid section ID #%" PRIu32,
				    fileName,
				    sect->name.c_str(),
				    i,
				    patch.pcSectionID
				);
			} else {
				patch.pcSection = fileSections[patch.pcSectionID].get();
			}
		}
	}

	// Give symbols' section pointers to their sections
	for (Symbol &sym : fileSymbols) {
		if (std::holds_alternative<Label>(sym.data)) {
			sym.linkToSection(*fileSections[std::get<Label>(sym.data).sectionID]);
		}
	}

	// Calling `sect_AddSection` invalidates the contents of `fileSections`!
	for (uint32_t i = 0; i < nbSections; ++i) {
		sect_AddSection(std::move(fileSections[i]));
	}

	// Fix symbols' section pointers to section "pieces"
	// This has to run **after** all the `sect_AddSection()` calls,
	// so that `sect_GetSection()` will work
	for (Symbol &sym : fileSymbols) {
		sym.fixSectionOffset();
	}
}

void obj_Setup(size_t nbFiles) {
	nodes.resize(nbFiles);
}
