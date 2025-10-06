// SPDX-License-Identifier: MIT

#ifndef RGBDS_BACKTRACE_HPP
#define RGBDS_BACKTRACE_HPP

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

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

template<typename T, typename M, typename N>
void trace_PrintBacktrace(std::vector<T> const &stack, M getName, N getLineNo) {
	size_t n = stack.size();
	if (n == 0) {
		return; // LCOV_EXCL_LINE
	}

	auto printLocation = [&](size_t i) {
		T const &item = stack[n - i - 1];
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

#endif // RGBDS_BACKTRACE_HPP
