#include "link/fstack.hpp"

#include <stdint.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "backtrace.hpp"
#include "helpers.hpp"
#include "linkdefs.hpp"

#include "link/warning.hpp"

using TraceNode = std::pair<std::string, uint32_t>;

static std::vector<TraceNode> backtrace(FileStackNode const &node, uint32_t curLineNo) {
	if (node.isQuiet && !tracing.loud) {
		if (node.parent) {
			// Quiet REPT nodes will pass their interior line number up to their parent,
			// which is more precise than the parent's own line number (since that will be
			// the line number of the "REPT?" or "FOR?" itself).
			return backtrace(*node.parent, node.type == NODE_REPT ? curLineNo : node.lineNo);
		}
		return {}; // LCOV_EXCL_LINE
	}

	if (!node.parent) {
		assume(node.type != NODE_REPT && std::holds_alternative<std::string>(node.data));
		return {
		    {node.name(), curLineNo}
		};
	}

	std::vector<TraceNode> traceNodes = backtrace(*node.parent, node.lineNo);
	if (std::holds_alternative<std::vector<uint32_t>>(node.data)) {
		assume(!traceNodes.empty()); // REPT nodes use their parent's name
		std::string reptChain = traceNodes.back().first;
		for (uint32_t iter : node.iters()) {
			reptChain.append("::REPT~");
			reptChain.append(std::to_string(iter));
		}
		traceNodes.emplace_back(reptChain, curLineNo);
	} else {
		traceNodes.emplace_back(node.name(), curLineNo);
	}
	return traceNodes;
}

void FileStackNode::printBacktrace(uint32_t curLineNo) const {
	trace_PrintBacktrace(
	    backtrace(*this, curLineNo),
	    [](TraceNode const &node) { return node.first.c_str(); },
	    [](TraceNode const &node) { return node.second; }
	);
}
