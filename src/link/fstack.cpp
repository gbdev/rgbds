// SPDX-License-Identifier: MIT

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

void FileStackNode::printBacktrace(uint32_t curLineNo) const {
	using TraceItem = std::pair<FileStackNode const *, uint32_t>;
	std::vector<TraceItem> items;
	for (TraceItem item{this, curLineNo};;) {
		auto &[node, itemLineNo] = item;
		bool loud = !node->isQuiet || tracing.loud;
		if (loud) {
			items.emplace_back(node, itemLineNo);
		}
		if (!node->parent) {
			assume(node->type != NODE_REPT && std::holds_alternative<std::string>(node->data));
			break;
		}
		if (loud || node->type != NODE_REPT) {
			// Quiet REPT nodes will pass their interior line number up to their parent,
			// which is more precise than the parent's own line number (since that will be
			// the line number of the "REPT?" or "FOR?" itself).
			itemLineNo = node->lineNo;
		}
		node = &*node->parent;
	}

	using TraceNode = std::pair<std::string, uint32_t>;
	std::vector<TraceNode> traceNodes;
	traceNodes.reserve(items.size());
	for (auto &[node, itemLineNo] : reversed(items)) {
		if (std::holds_alternative<std::vector<uint32_t>>(node->data)) {
			// REPT nodes use their first non-REPT parent's name (which may be a quiet node and
			// thus not in `traceNodes`), followed by their REPT parents' iteration counts,
			// followed by their own iteration count.
			std::string reptName;
			for (FileStackNode const *ancestor = node->parent;; ancestor = ancestor->parent) {
				// The top level context cannot be a REPT (guaranteed by `readFileStackNode`)
				assume(ancestor != nullptr);
				if (ancestor->type != NODE_REPT) {
					reptName.append(ancestor->name());
					break;
				}
			}
			for (uint32_t iter : node->iters()) {
				reptName.append(NODE_SEPARATOR REPT_NODE_PREFIX);
				reptName.append(std::to_string(iter));
			}
			traceNodes.emplace_back(std::move(reptName), itemLineNo);
		} else {
			traceNodes.emplace_back(node->name(), itemLineNo);
		}
	}

	trace_PrintBacktrace(
	    traceNodes,
	    [](TraceNode const &node) { return node.first.c_str(); },
	    [](TraceNode const &node) { return node.second; }
	);
}
