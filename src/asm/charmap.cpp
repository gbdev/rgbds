// SPDX-License-Identifier: MIT

#include "asm/charmap.hpp"

#include <deque>
#include <map>
#include <optional>
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "extern/utf8decoder.hpp"
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
};

// Traverse the trie depth-first to derive the character mappings in definition order
template<typename F>
bool forEachChar(Charmap const &charmap, F callback) {
	// clang-format off: nested initializers
	for (std::stack<std::pair<size_t, std::string>> prefixes({{0, ""}}); !prefixes.empty();) {
		// clang-format on
		auto [nodeIdx, mapping] = std::move(prefixes.top());
		prefixes.pop();
		CharmapNode const &node = charmap.nodes[nodeIdx];
		if (node.isTerminal() && !callback(nodeIdx, mapping)) {
			return false;
		}
		for (unsigned c = 0; c < std::size(node.next); ++c) {
			if (size_t nextIdx = node.next[c]; nextIdx) {
				prefixes.push({nextIdx, mapping + static_cast<char>(c)});
			}
		}
	}
	return true;
}

static std::deque<Charmap> charmapList;
static std::unordered_map<std::string, size_t> charmapMap; // Indexes into `charmapList`

static Charmap *currentCharmap;
static std::stack<Charmap *> charmapStack;

bool charmap_ForEach(
    void (*mapFunc)(std::string const &),
    void (*charFunc)(std::string const &, std::vector<int32_t>)
) {
	for (Charmap const &charmap : charmapList) {
		std::map<size_t, std::string> mappings;
		forEachChar(charmap, [&mappings](size_t nodeIdx, std::string const &mapping) {
			mappings[nodeIdx] = mapping;
			return true;
		});

		mapFunc(charmap.name);
		for (auto const &[nodeIdx, mapping] : mappings) {
			charFunc(mapping, charmap.nodes[nodeIdx].value);
		}
	}
	return !charmapList.empty();
}

void charmap_New(std::string const &name, std::string const *baseName) {
	size_t baseIdx = SIZE_MAX;

	if (baseName != nullptr) {
		if (auto search = charmapMap.find(*baseName); search == charmapMap.end()) {
			error("Undefined base charmap `%s`", baseName->c_str());
		} else {
			baseIdx = search->second;
		}
	}

	if (charmapMap.find(name) != charmapMap.end()) {
		error("Charmap `%s` is already defined", name.c_str());
		return;
	}

	// Init the new charmap's fields
	charmapMap[name] = charmapList.size();
	Charmap &charmap = charmapList.emplace_back();

	if (baseIdx != SIZE_MAX) {
		charmap.nodes = charmapList[baseIdx].nodes; // Copies `charmapList[baseIdx].nodes`
	} else {
		charmap.nodes.emplace_back(); // Zero-init the root node
	}

	charmap.name = name;

	currentCharmap = &charmap;
}

void charmap_Set(std::string const &name) {
	if (auto search = charmapMap.find(name); search == charmapMap.end()) {
		error("Undefined charmap `%s`", name.c_str());
	} else {
		currentCharmap = &charmapList[search->second];
	}
}

void charmap_Push() {
	charmapStack.push(currentCharmap);
}

void charmap_Pop() {
	if (charmapStack.empty()) {
		error("No entries in the charmap stack");
		return;
	}

	currentCharmap = charmapStack.top();
	charmapStack.pop();
}

void charmap_CheckStack() {
	if (!charmapStack.empty()) {
		warning(WARNING_UNMATCHED_DIRECTIVE, "`PUSHC` without corresponding `POPC`");
	}
}

void charmap_Add(std::string const &mapping, std::vector<int32_t> &&value) {
	if (mapping.empty()) {
		error("Cannot map an empty string");
		return;
	}

	Charmap &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (char c : mapping) {
		size_t &nextIdxRef = charmap.nodes[nodeIdx].next[static_cast<uint8_t>(c)];
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

	if (node.isTerminal()) {
		warning(WARNING_CHARMAP_REDEF, "Overriding charmap mapping");
	}

	std::swap(node.value, value);
}

bool charmap_HasChar(std::string const &mapping) {
	Charmap const &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (char c : mapping) {
		nodeIdx = charmap.nodes[nodeIdx].next[static_cast<uint8_t>(c)];

		if (!nodeIdx) {
			return false;
		}
	}

	return charmap.nodes[nodeIdx].isTerminal();
}

static CharmapNode const *charmapEntry(std::string const &mapping) {
	Charmap const &charmap = *currentCharmap;
	size_t nodeIdx = 0;

	for (char c : mapping) {
		nodeIdx = charmap.nodes[nodeIdx].next[static_cast<uint8_t>(c)];

		if (!nodeIdx) {
			return nullptr;
		}
	}

	return &charmap.nodes[nodeIdx];
}

size_t charmap_CharSize(std::string const &mapping) {
	CharmapNode const *node = charmapEntry(mapping);
	return node && node->isTerminal() ? node->value.size() : 0;
}

std::optional<int32_t> charmap_CharValue(std::string const &mapping, size_t idx) {
	if (CharmapNode const *node = charmapEntry(mapping);
	    node && node->isTerminal() && idx < node->value.size()) {
		return node->value[idx];
	}
	return std::nullopt;
}

std::vector<int32_t> charmap_Convert(std::string const &input) {
	std::vector<int32_t> output;
	for (std::string_view inputView = input; charmap_ConvertNext(inputView, &output);) {}
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
		nodeIdx = charmap.nodes[nodeIdx].next[static_cast<uint8_t>(input[inputIdx])];

		if (!nodeIdx) {
			break;
		}

		++inputIdx; // Consume that char

		if (charmap.nodes[nodeIdx].isTerminal()) {
			matchIdx = nodeIdx; // This node matches, register it
			rewindDistance = 0; // If no longer match is found, rewind here
		} else {
			++rewindDistance;
		}
	}

	// We are at a dead end (either because we reached the end of input, or of the trie),
	// so rewind up to the last match, and output.
	inputIdx -= rewindDistance; // This will rewind all the way if no match found

	size_t matchLen = 0;
	if (matchIdx) { // A match was found, use it
		std::vector<int32_t> const &value = charmap.nodes[matchIdx].value;

		if (output) {
			output->insert(output->end(), RANGE(value));
		}

		matchLen = value.size();
	} else if (inputIdx < input.length()) { // No match found, but there is some input left
		size_t codepointLen = 0;
		// This will write the codepoint's value to `output`, little-endian
		for (uint32_t state = UTF8_ACCEPT, codepoint = 0;
		     inputIdx + codepointLen < input.length();) {
			if (decode(&state, &codepoint, input[inputIdx + codepointLen]) == UTF8_REJECT) {
				error("Input string is not valid UTF-8");
				codepointLen = 1;
				break;
			}
			++codepointLen;
			if (state == UTF8_ACCEPT) {
				break;
			}
		}

		if (output) {
			output->insert(
			    output->end(), input.data() + inputIdx, input.data() + inputIdx + codepointLen
			);
		}

		// Warn if this character is not mapped but any others are
		if (int firstChar = input[inputIdx]; charmap.nodes.size() > 1) {
			warning(WARNING_UNMAPPED_CHAR_1, "Unmapped character %s", printChar(firstChar));
		} else if (charmap.name != DEFAULT_CHARMAP_NAME) {
			warning(
			    WARNING_UNMAPPED_CHAR_2,
			    "Unmapped character %s not in `" DEFAULT_CHARMAP_NAME "` charmap",
			    printChar(firstChar)
			);
		}

		inputIdx += codepointLen;
		matchLen = codepointLen;
	}

	input = input.substr(inputIdx);
	return matchLen;
}

std::string charmap_Reverse(std::vector<int32_t> const &value, bool &unique) {
	Charmap const &charmap = *currentCharmap;
	std::string revMapping;
	unique = forEachChar(charmap, [&](size_t nodeIdx, std::string const &mapping) {
		if (charmap.nodes[nodeIdx].value == value) {
			if (revMapping.empty()) {
				revMapping = mapping;
			} else {
				revMapping.clear();
				return false;
			}
		}
		return true;
	});
	return revMapping;
}
