#include "link/fstack.hpp"

#include <inttypes.h>
#include <utility>

#include "backtrace.hpp"

#include "link/warning.hpp"

std::vector<std::pair<std::string, uint32_t>> FileStackNode::backtrace(uint32_t curLineNo) const {
	if (std::holds_alternative<std::vector<uint32_t>>(data)) {
		assume(parent); // REPT nodes use their parent's name
		std::vector<std::pair<std::string, uint32_t>> nodes = parent->backtrace(lineNo);
		assume(!nodes.empty());
		std::string reptChain = nodes.back().first;
		for (uint32_t iter : iters()) {
			reptChain.append("::REPT~");
			reptChain.append(std::to_string(iter));
		}
		nodes.emplace_back(reptChain, curLineNo);
		return nodes;
	} else if (parent) {
		std::vector<std::pair<std::string, uint32_t>> nodes = parent->backtrace(lineNo);
		nodes.emplace_back(name(), curLineNo);
		return nodes;
	} else {
		return {
		    {name(), curLineNo}
		};
	}
}

void FileStackNode::printBacktrace(uint32_t curLineNo) const {
	trace_PrintBacktrace(
	    backtrace(curLineNo),
	    [](std::pair<std::string, uint32_t> const &node) { return node.first.c_str(); },
	    [](std::pair<std::string, uint32_t> const &node) { return node.second; }
	);
}
