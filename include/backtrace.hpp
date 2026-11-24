// SPDX-License-Identifier: MIT

#ifndef RGBDS_BACKTRACE_HPP
#define RGBDS_BACKTRACE_HPP

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "helpers.hpp"   // InvocableR
#include "itertools.hpp" // reversed
#include "linkdefs.hpp"  // NODE_REPT
#include "style.hpp"

#define TRACE_SEPARATOR  "<-"
#define NODE_SEPARATOR   "::"
#define REPT_NODE_PREFIX "REPT~"

struct Tracing {
	uint64_t depth = 0;
	bool collapse = false;
	bool loud = false;
};

extern Tracing tracing;

bool trace_ParseTraceDepth(char const *arg);

template<typename NodeT>
void trace_PrintBacktrace(
    std::vector<NodeT> const &stack,
    InvocableR<char const *, NodeT const &> auto getName,
    InvocableR<uint32_t, NodeT const &> auto getLineNo
) {
	size_t n = stack.size();
	if (n == 0) {
		return; // LCOV_EXCL_LINE
	}

	auto printLocation = [&](size_t i) {
		NodeT const &item = stack[n - i - 1];
		style_Reset(stderr);
		if (!tracing.collapse) {
			fputs("   ", stderr); // Just three spaces; the fourth will be printed next
		}
		fprintf(stderr, " %s ", i == 0 ? "at" : TRACE_SEPARATOR);
		style_Set(stderr, STYLE_CYAN, true);
		fputs(getName(item), stderr);
		style_Set(stderr, STYLE_CYAN, false);
		fprintf(stderr, "(%" PRIu32 ")", getLineNo(item));
		if (!tracing.collapse) {
			putc('\n', stderr);
		}
	};

	if (tracing.collapse) {
		fputs("   ", stderr); // Just three spaces; the fourth will be handled by the loop
	}

	if (tracing.depth == 0 || static_cast<size_t>(tracing.depth) >= n) {
		for (size_t i = 0; i < n; ++i) {
			printLocation(i);
		}
	} else {
		size_t last = tracing.depth / 2;
		size_t first = tracing.depth - last;
		size_t skipped = n - tracing.depth;
		for (size_t i = 0; i < first; ++i) {
			printLocation(i);
		}
		style_Reset(stderr);

		if (tracing.collapse) {
			fputs(" " TRACE_SEPARATOR, stderr);
		} else {
			fputs("   ", stderr); // Just three spaces; the fourth will be printed next
		}
		fprintf(stderr, " ...%zu more%s", skipped, last ? "..." : "");
		if (!tracing.collapse) {
			putc('\n', stderr);
		}

		for (size_t i = n - last; i < n; ++i) {
			printLocation(i);
		}
	}

	if (tracing.collapse) {
		putc('\n', stderr);
	}
	style_Reset(stderr);
}

template<typename NodeT>
void trace_PrintFstackNodeBacktrace(NodeT const &fstackNode, uint32_t curLineNo) {
	using TraceItem = std::pair<NodeT const *, uint32_t>;
	std::vector<TraceItem> items;
	for (TraceItem item{&fstackNode, curLineNo};;) {
		auto &[node, itemLineNo] = item;
		bool loud = !node->isQuiet || tracing.loud;
		if (loud) {
			items.emplace_back(node, itemLineNo);
		}
		if (!node->parentNode()) {
			assume(node->type != NODE_REPT && std::holds_alternative<std::string>(node->data));
			break;
		}
		if (loud || node->type != NODE_REPT) {
			// Quiet REPT nodes will pass their interior line number up to their parent,
			// which is more precise than the parent's own line number (since that will be
			// the line number of the "REPT?" or "FOR?" itself).
			itemLineNo = node->lineNo;
		}
		node = &*node->parentNode();
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
			for (NodeT const *ancestor = node->parentNode();; ancestor = ancestor->parentNode()) {
				assume(ancestor != nullptr); // The top level context cannot be a REPT
				if (ancestor->type != NODE_REPT) {
					reptName.append(ancestor->name());
					break;
				}
			}
			for (uint32_t iter : reversed(node->iters())) {
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

#endif // RGBDS_BACKTRACE_HPP
