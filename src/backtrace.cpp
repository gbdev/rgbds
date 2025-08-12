// SPDX-License-Identifier: MIT

#include "backtrace.hpp"

#include <stdlib.h> // strtoul

#include "platform.hpp" // strcasecmp

uint64_t traceDepth = 0;

bool trace_ParseTraceDepth(char const *arg) {
	if (!strcasecmp(arg, "collapse")) {
		traceDepth = TRACE_COLLAPSE;
		return true;
	}

	char *endptr;
	traceDepth = strtoul(arg, &endptr, 0);

	return arg[0] != '\0' && *endptr == '\0' && traceDepth != TRACE_COLLAPSE;
}
