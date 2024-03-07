/* SPDX-License-Identifier: MIT */

#include "asm/charmap.hpp"

#include <errno.h>
#include <map>
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "util.hpp"

#include "asm/main.hpp"
#include "asm/output.hpp"
#include "asm/warning.hpp"

// Charmaps are stored using a structure known as "trie".
// Essentially a tree, where each nodes stores a single character's worth of info:
// whether there exists a mapping that ends at the current character,
struct CharmapNode {
	bool isTerminal; // Whether there exists a mapping that ends here
	uint8_t value;   // If the above is true, its corresponding value
	// This MUST be indexes and not pointers, because pointers get invalidated by reallocation!
	size_t next[255]; // Indexes of where to go next, 0 = nowhere
};

struct Charmap {
	std::string name;
	std::vector<CharmapNode> nodes; // first node is reserved for the root node
};

static std::map<std::string, Charmap> charmaps;

static Charmap *currentCharmap;
std::stack<Charmap *> charmapStack;

void charmap_New(char const *name, char const *baseName) {
	Charmap *base = nullptr;

	if (baseName != nullptr) {
		auto search = charmaps.find(baseName);

		if (search == charmaps.end())
			error("Base charmap '%s' doesn't exist\n", baseName);
		else
			base = &search->second;
	}

	if (charmaps.find(name) != charmaps.end()) {
		error("Charmap '%s' already exists\n", name);
		return;
	}

	// Init the new charmap's fields
	Charmap &charmap = charmaps[name];

	if (base)
		charmap.nodes = base->nodes; // Copies `base->nodes`
	else
		charmap.nodes.emplace_back(); // Zero-init the root node
	charmap.name = name;

	currentCharmap = &charmap;
}

void charmap_Set(char const *name) {
	auto search = charmaps.find(name);

	if (search == charmaps.end())
		error("Charmap '%s' doesn't exist\n", name);
	else
		currentCharmap = &search->second;
}

void charmap_Push() {
	charmapStack.push(currentCharmap);
}

void charmap_Pop() {
	if (charmapStack.empty()) {
		error("No entries in the charmap stack\n");
		return;
	}

	currentCharmap = charmapStack.top();
	charmapStack.pop();
}

void charmap_Add(char const *mapping, uint8_t value) {
	Charmap &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (; *mapping; mapping++) {
		size_t &nextIdxRef = charmap.nodes[nodeIdx].next[(uint8_t)*mapping - 1];
		size_t nextIdx = nextIdxRef;

		if (!nextIdx) {
			// Switch to and zero-init the new node
			nextIdxRef = charmap.nodes.size();
			nextIdx = nextIdxRef;
			// This may reallocate `charmap.nodes` and invalidate `nextIdxRef`,
			// which is why we keep the actual value in `nextIdx`
			charmap.nodes.emplace_back();
		}

		nodeIdx = nextIdx;
	}

	CharmapNode &node = charmap.nodes[nodeIdx];

	if (node.isTerminal)
		warning(WARNING_CHARMAP_REDEF, "Overriding charmap mapping\n");

	node.isTerminal = true;
	node.value = value;
}

bool charmap_HasChar(char const *input) {
	Charmap const &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (; *input; input++) {
		nodeIdx = charmap.nodes[nodeIdx].next[(uint8_t)*input - 1];

		if (!nodeIdx)
			return false;
	}

	return charmap.nodes[nodeIdx].isTerminal;
}

void charmap_Convert(char const *input, std::vector<uint8_t> &output) {
	while (charmap_ConvertNext(input, &output))
		;
}

size_t charmap_ConvertNext(char const *&input, std::vector<uint8_t> *output) {
	// The goal is to match the longest mapping possible.
	// For that, advance through the trie with each character read.
	// If that would lead to a dead end, rewind characters until the last match, and output.
	// If no match, read a UTF-8 codepoint and output that.
	Charmap const &charmap = *currentCharmap;
	size_t matchIdx = 0;
	size_t rewindDistance = 0;

	for (size_t nodeIdx = 0; *input;) {
		nodeIdx = charmap.nodes[nodeIdx].next[(uint8_t)*input - 1];

		if (!nodeIdx)
			break;

		input++; // Consume that char

		if (charmap.nodes[nodeIdx].isTerminal) {
			matchIdx = nodeIdx; // This node matches, register it
			rewindDistance = 0; // If no longer match is found, rewind here
		} else {
			rewindDistance++;
		}
	}

	// We are at a dead end (either because we reached the end of input, or of the trie),
	// so rewind up to the last match, and output.
	input -= rewindDistance; // This will rewind all the way if no match found

	if (matchIdx) { // A match was found, use it
		if (output)
			output->push_back(charmap.nodes[matchIdx].value);

		return 1;

	} else if (*input) { // No match found, but there is some input left
		int firstChar = *input;
		// This will write the codepoint's value to `output`, little-endian
		size_t codepointLen = readUTF8Char(output, input);

		if (codepointLen == 0)
			error("Input string is not valid UTF-8\n");

		// OK because UTF-8 has no NUL in multi-byte chars
		input += codepointLen;

		// Warn if this character is not mapped but any others are
		if (charmap.nodes.size() > 1)
			warning(WARNING_UNMAPPED_CHAR_1, "Unmapped character %s\n", printChar(firstChar));
		else if (charmap.name != DEFAULT_CHARMAP_NAME)
			warning(
			    WARNING_UNMAPPED_CHAR_2,
			    "Unmapped character %s not in " DEFAULT_CHARMAP_NAME " charmap\n",
			    printChar(firstChar)
			);

		return codepointLen;

	} else { // End of input
		return 0;
	}
}
