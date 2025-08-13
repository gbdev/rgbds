// SPDX-License-Identifier: MIT

#include "backtrace.hpp"

#include <stdlib.h> // strtoul

#include "platform.hpp" // strcasecmp

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
		char *endptr;
		tracing.depth = strtoul(arg, &endptr, 0);
		return arg[0] != '\0' && *endptr == '\0';
	}
}
