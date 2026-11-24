// SPDX-License-Identifier: MIT

#include "link/fstack.hpp"

#include <stdint.h>

#include "backtrace.hpp"

void FileStackNode::printBacktrace(uint32_t curLineNo) const {
	trace_PrintFstackNodeBacktrace(*this, curLineNo);
}
