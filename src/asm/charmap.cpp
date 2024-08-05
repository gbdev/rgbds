/* SPDX-License-Identifier: MIT */

#include "asm/charmap.hpp"

#include <deque>
#include <stack>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

#include "helpers.hpp"
#include "util.hpp"

#include "asm/warning.hpp"

// Charmaps are stored using a structure known as "trie".
// Essentially a tree, where each nodes stores a single character's worth of info:
// whether there exists a mapping that ends at the current character,
struct CharmapNode {
	std::vector<int32_t> value; // The mapped value, if there exists a mapping that ends here
	// These MUST be indexes and not pointers, because pointers get invalidated by reallocation!
	size_t next[256]; // Indexes of where to go next, 0 = nowhere

	bool isTerminal() const { return !value.empty(); }
};

struct Charmap {
	std::string name;
	std::vector<CharmapNode> nodes; // first node is reserved for the root node
	// FIXME: strictly speaking, this is redundant, we could walk the trie to get mappings instead
	std::unordered_map<size_t, std::string> mappings; // keys are indexes of terminal nodes
};

static std::deque<Charmap> charmapList;
static std::unordered_map<std::string, size_t> charmapMap; // Indexes into `charmapList`

static Charmap *currentCharmap;
std::stack<Charmap *> charmapStack;

bool charmap_ForEach(
    void (*mapFunc)(std::string const &),
    void (*charFunc)(std::string const &, std::vector<int32_t>)
) {
	for (Charmap &charmap : charmapList) {
		mapFunc(charmap.name);
		for (size_t i = 0; i < charmap.nodes.size(); ++i) {
			if (CharmapNode const &node = charmap.nodes[i]; node.isTerminal())
				charFunc(charmap.mappings[i], node.value);
		}
	}
	return !charmapList.empty();
}

void charmap_New(std::string const &name, std::string const *baseName) {
	size_t baseIdx = (size_t)-1;

	if (baseName != nullptr) {
		if (auto search = charmapMap.find(*baseName); search == charmapMap.end())
			error("Base charmap '%s' doesn't exist\n", baseName->c_str());
		else
			baseIdx = search->second;
	}

	if (charmapMap.find(name) != charmapMap.end()) {
		error("Charmap '%s' already exists\n", name.c_str());
		return;
	}

	// Init the new charmap's fields
	charmapMap[name] = charmapList.size();
	Charmap &charmap = charmapList.emplace_back();

	if (baseIdx != (size_t)-1)
		charmap.nodes = charmapList[baseIdx].nodes; // Copies `charmapList[baseIdx].nodes`
	else
		charmap.nodes.emplace_back(); // Zero-init the root node

	charmap.name = name;

	currentCharmap = &charmap;
}

void charmap_Set(std::string const &name) {
	if (auto search = charmapMap.find(name); search == charmapMap.end())
		error("Charmap '%s' doesn't exist\n", name.c_str());
	else
		currentCharmap = &charmapList[search->second];
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

void charmap_Add(std::string const &mapping, std::vector<int32_t> &&value) {
	Charmap &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (char c : mapping) {
		size_t &nextIdxRef = charmap.nodes[nodeIdx].next[(uint8_t)c];
		size_t nextIdx = nextIdxRef;

		if (!nextIdx) {
			// Switch to and zero-init the new node
			nextIdxRef = charmap.nodes.size();
			nextIdx = nextIdxRef;
			// Save the mapping of this node
			charmap.mappings[charmap.nodes.size()] = mapping;
			// This may reallocate `charmap.nodes` and invalidate `nextIdxRef`,
			// which is why we keep the actual value in `nextIdx`
			charmap.nodes.emplace_back();
		}

		nodeIdx = nextIdx;
	}

	CharmapNode &node = charmap.nodes[nodeIdx];

	if (node.isTerminal())
		warning(WARNING_CHARMAP_REDEF, "Overriding charmap mapping\n");

	std::swap(node.value, value);
}

bool charmap_HasChar(std::string const &input) {
	Charmap const &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (char c : input) {
		nodeIdx = charmap.nodes[nodeIdx].next[(uint8_t)c];

		if (!nodeIdx)
			return false;
	}

	return charmap.nodes[nodeIdx].isTerminal();
}

std::vector<int32_t> charmap_Convert(std::string const &input) {
	std::vector<int32_t> output;
	for (std::string_view inputView = input; charmap_ConvertNext(inputView, &output);)
		;
	return output;
}

size_t charmap_ConvertNext(std::string_view &input, std::vector<int32_t> *output) {
	// The goal is to match the longest mapping possible.
	// For that, advance through the trie with each character read.
	// If that would lead to a dead end, rewind characters until the last match, and output.
	// If no match, read a UTF-8 codepoint and output that.
	Charmap const &charmap = *currentCharmap;
	size_t matchIdx = 0;
	size_t rewindDistance = 0;
	size_t inputIdx = 0;

	for (size_t nodeIdx = 0; inputIdx < input.length();) {
		nodeIdx = charmap.nodes[nodeIdx].next[(uint8_t)input[inputIdx]];

		if (!nodeIdx)
			break;

		inputIdx++; // Consume that char

		if (charmap.nodes[nodeIdx].isTerminal()) {
			matchIdx = nodeIdx; // This node matches, register it
			rewindDistance = 0; // If no longer match is found, rewind here
		} else {
			rewindDistance++;
		}
	}

	// We are at a dead end (either because we reached the end of input, or of the trie),
	// so rewind up to the last match, and output.
	inputIdx -= rewindDistance; // This will rewind all the way if no match found

	size_t matchLen = 0;
	if (matchIdx) { // A match was found, use it
		std::vector<int32_t> const &value = charmap.nodes[matchIdx].value;

		if (output)
			output->insert(output->end(), RANGE(value));

		matchLen = value.size();
	} else if (inputIdx < input.length()) { // No match found, but there is some input left
		int firstChar = input[inputIdx];
		// This will write the codepoint's value to `output`, little-endian
		size_t codepointLen = readUTF8Char(output, input.data() + inputIdx);

		if (codepointLen == 0)
			error("Input string is not valid UTF-8\n");

		// Warn if this character is not mapped but any others are
		if (charmap.nodes.size() > 1)
			warning(WARNING_UNMAPPED_CHAR_1, "Unmapped character %s\n", printChar(firstChar));
		else if (charmap.name != DEFAULT_CHARMAP_NAME)
			warning(
			    WARNING_UNMAPPED_CHAR_2,
			    "Unmapped character %s not in " DEFAULT_CHARMAP_NAME " charmap\n",
			    printChar(firstChar)
			);

		inputIdx += codepointLen;
		matchLen = codepointLen;
	}

	input = input.substr(inputIdx);
	return matchLen;
}
