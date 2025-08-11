#include "link/fstack.hpp"

#include <inttypes.h>
#include <utility>

#include "style.hpp"

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

static void printNode(std::pair<std::string, uint32_t> const &node) {
	style_Set(stderr, STYLE_CYAN, true);
	fputs(node.first.c_str(), stderr);
	style_Set(stderr, STYLE_CYAN, false);
	fprintf(stderr, "(%" PRIu32 ")", node.second);
}

void FileStackNode::printBacktrace(uint32_t curLineNo) const {
	std::vector<std::pair<std::string, uint32_t>> nodes = backtrace(curLineNo);
	size_t n = nodes.size();

	if (warnings.traceDepth == TRACE_COLLAPSE) {
		fputs("   ", stderr); // Just three spaces; the fourth will be handled by the loop
		for (size_t i = 0; i < n; ++i) {
			style_Reset(stderr);
			fprintf(stderr, " %s ", i == 0 ? "at" : "<-");
			printNode(nodes[n - i - 1]);
		}
		putc('\n', stderr);
	} else if (warnings.traceDepth == 0 || static_cast<size_t>(warnings.traceDepth) >= n) {
		for (size_t i = 0; i < n; ++i) {
			style_Reset(stderr);
			fprintf(stderr, "    %s ", i == 0 ? "at" : "<-");
			printNode(nodes[n - i - 1]);
			putc('\n', stderr);
		}
	} else {
		size_t last = warnings.traceDepth / 2;
		size_t first = warnings.traceDepth - last;
		size_t skipped = n - warnings.traceDepth;
		for (size_t i = 0; i < first; ++i) {
			style_Reset(stderr);
			fprintf(stderr, "    %s ", i == 0 ? "at" : "<-");
			printNode(nodes[n - i - 1]);
			putc('\n', stderr);
		}
		style_Reset(stderr);
		fprintf(stderr, "    ...%zu more%s\n", skipped, last ? "..." : "");
		for (size_t i = n - last; i < n; ++i) {
			style_Reset(stderr);
			fputs("    <- ", stderr);
			printNode(nodes[n - i - 1]);
			putc('\n', stderr);
		}
	}

	style_Reset(stderr);
}
