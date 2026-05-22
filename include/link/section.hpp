// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_SECTION_HPP
#define RGBDS_LINK_SECTION_HPP

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include "linkdefs.hpp"

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
	FileStackNode const *src;
	int32_t lineNo;
	std::vector<uint8_t> data; // Array of size `size`, or 0 if `type` does not have data
	std::vector<Patch> patches;
	// Extra info computed during linking
	std::vector<Symbol> *fileSymbols;
	std::vector<Symbol *> symbols;
	std::unique_ptr<Section> nextPiece; // The next fragment or union "piece" of this section

private:
	// Template class for both const and non-const iterators over the "pieces" of this section
	template<typename SectionT>
	class PiecesIterable {
		SectionT *_firstPiece;

		class Iterator {
			SectionT *_piece;

		public:
			explicit Iterator(SectionT *piece) : _piece(piece) {}

			Iterator &operator++() {
				_piece = _piece->nextPiece.get();
				return *this;
			}

			SectionT &operator*() const { return *_piece; }

			bool operator==(Iterator const &rhs) const { return _piece == rhs._piece; }
		};

	public:
		explicit PiecesIterable(SectionT *firstPiece) : _firstPiece(firstPiece) {}

		Iterator begin() { return Iterator(_firstPiece); }
		Iterator end() { return Iterator(nullptr); }
	};

public:
	PiecesIterable<Section> pieces() { return PiecesIterable(this); }
	PiecesIterable<Section const> pieces() const { return PiecesIterable(this); }
};

// Execute a callback for each section currently registered.
// This is to avoid exposing the data structure in which sections are stored.
void sect_ForEach(void (*callback)(Section &));

// Registers a section to be processed.
void sect_AddSection(std::unique_ptr<Section> &&section);

// Finds a section by its name.
Section *sect_GetSection(std::string const &name);

// Checks if all sections meet reasonable criteria, such as max size
void sect_DoSanityChecks();

#endif // RGBDS_LINK_SECTION_HPP
