// SPDX-License-Identifier: MIT

#include "backtrace.hpp"

#include <optional>
#include <stdint.h>

#include "platform.hpp" // strcasecmp
#include "util.hpp"     // parseWholeNumber

Tracing tracing;

bool trace_ParseTraceDepth(char const *arg) {
	if (!strcasecmp(arg, "collapse")) {
		tracing.collapse = true;
		return true;
	} else if (!strcasecmp(arg, "no-collapse")) {
		tracing.collapse = false;
		return true;
	} else if (!strcasecmp(arg, "all")) {
		tracing.loud = true;
		return true;
	} else if (!strcasecmp(arg, "no-all")) {
		tracing.loud = false;
		return true;
	} else {
		std::optional<uint64_t> depth = parseWholeNumber(arg);
		if (depth) {
			tracing.depth = *depth;
		}
		return depth.has_value();
	}
}
