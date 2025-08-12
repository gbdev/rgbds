// SPDX-License-Identifier: MIT

#ifndef RGBDS_BACKTRACE_HPP
#define RGBDS_BACKTRACE_HPP

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "style.hpp"

static constexpr uint64_t TRACE_COLLAPSE = UINT64_MAX;

extern uint64_t traceDepth;

bool trace_ParseTraceDepth(char const *arg);

template<typename T, typename M, typename N>
void trace_PrintBacktrace(std::vector<T> const &stack, M getName, N getLineNo) {
	auto printLocation = [&](T const &item) {
		style_Set(stderr, STYLE_CYAN, true);
		fputs(getName(item), stderr);
		style_Set(stderr, STYLE_CYAN, false);
		fprintf(stderr, "(%" PRIu32 ")", getLineNo(item));
	};

	size_t n = stack.size();

	if (traceDepth == TRACE_COLLAPSE) {
		fputs("   ", stderr); // Just three spaces; the fourth will be handled by the loop
		for (size_t i = 0; i < n; ++i) {
			style_Reset(stderr);
			fprintf(stderr, " %s ", i == 0 ? "at" : "<-");
			printLocation(stack[n - i - 1]);
		}
		putc('\n', stderr);
	} else if (traceDepth == 0 || static_cast<size_t>(traceDepth) >= n) {
		for (size_t i = 0; i < n; ++i) {
			style_Reset(stderr);
			fprintf(stderr, "    %s ", i == 0 ? "at" : "<-");
			printLocation(stack[n - i - 1]);
			putc('\n', stderr);
		}
	} else {
		size_t last = traceDepth / 2;
		size_t first = traceDepth - last;
		size_t skipped = n - traceDepth;
		for (size_t i = 0; i < first; ++i) {
			style_Reset(stderr);
			fprintf(stderr, "    %s ", i == 0 ? "at" : "<-");
			printLocation(stack[n - i - 1]);
			putc('\n', stderr);
		}
		style_Reset(stderr);
		fprintf(stderr, "    ...%zu more%s\n", skipped, last ? "..." : "");
		for (size_t i = n - last; i < n; ++i) {
			style_Reset(stderr);
			fputs("    <- ", stderr);
			printLocation(stack[n - i - 1]);
			putc('\n', stderr);
		}
	}

	style_Reset(stderr);
}

#endif // RGBDS_BACKTRACE_HPP
