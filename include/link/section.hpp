/* SPDX-License-Identifier: MIT */

// Declarations manipulating symbols
#ifndef RGBDS_LINK_SECTION_H
#define RGBDS_LINK_SECTION_H

// GUIDELINE: external code MUST NOT BE AWARE of the data structure used!

#include <stdint.h>
#include <string>
#include <vector>

#include "linkdefs.hpp"

#include "link/main.hpp"

struct FileStackNode;
struct Section;
struct Symbol;

struct Patch {
	FileStackNode const *src;
	uint32_t lineNo;
	uint32_t offset;
	Section const *pcSection;
	uint32_t pcSectionID;
	uint32_t pcOffset;
	PatchType type;
	std::vector<uint8_t> rpnExpression;
};

struct Section {
	// Info contained in the object files
	std::string name;
	uint16_t size;
	uint16_t offset;
	SectionType type;
	SectionModifier modifier;
	bool isAddressFixed;
	// This `struct`'s address in ROM.
	// Importantly for fragments, this does not include `offset`!
	uint16_t org;
	bool isBankFixed;
	uint32_t bank;
	bool isAlignFixed;
	uint16_t alignMask;
	uint16_t alignOfs;
	std::vector<uint8_t> data; // Array of size `size`, or 0 if `type` does not have data
	std::vector<Patch> patches;
	// Extra info computed during linking
	std::vector<Symbol> *fileSymbols;
	std::vector<Symbol *> symbols;
	Section *nextu; // The next "component" of this unionized sect
};

/*
 * Execute a callback for each section currently registered.
 * This is to avoid exposing the data structure in which sections are stored.
 * @param callback The function to call for each structure.
 */
void sect_ForEach(void (*callback)(Section &));

/*
 * Registers a section to be processed.
 * @param section The section to register.
 */
void sect_AddSection(Section &section);

/*
 * Finds a section by its name.
 * @param name The name of the section to look for
 * @return A pointer to the section, or `nullptr` if it wasn't found
 */
Section *sect_GetSection(std::string const &name);

/*
 * Checks if all sections meet reasonable criteria, such as max size
 */
void sect_DoSanityChecks();

#endif // RGBDS_LINK_SECTION_H
