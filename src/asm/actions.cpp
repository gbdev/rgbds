// SPDX-License-Identifier: MIT

#include "asm/actions.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern/utf8decoder.hpp"
#include "helpers.hpp"

#include "asm/charmap.hpp"
#include "asm/format.hpp"
#include "asm/fstack.hpp"
#include "asm/symbol.hpp"
#include "asm/warning.hpp"

std::optional<std::string> act_ReadFile(std::string const &name, uint32_t maxLen) {
	FILE *file = nullptr;
	if (std::optional<std::string> fullPath = fstk_FindFile(name); fullPath) {
		file = fopen(fullPath->c_str(), "rb");
	}
	if (!file) {
		if (fstk_FileError(name, "READFILE")) {
			// If `fstk_FileError` returned true due to `-MG`, we should abort due to a
			// missing file, so return `std::nullopt`, which tells the caller to `YYACCEPT`
			return std::nullopt;
		}
		return "";
	}
	Defer closeFile{[&] { fclose(file); }};

	size_t readSize = maxLen;
	if (fseek(file, 0, SEEK_END) == 0) {
		// If the file is seekable and shorter than the max length,
		// just read as many bytes as there are
		if (long fileSize = ftell(file); static_cast<size_t>(fileSize) < readSize) {
			readSize = fileSize;
		}
		fseek(file, 0, SEEK_SET);
	} else if (errno != ESPIPE) {
		error("Error determining size of READFILE file '%s': %s", name.c_str(), strerror(errno));
	}

	std::string contents;
	contents.resize(readSize);

	if (fread(&contents[0], 1, readSize, file) < readSize || ferror(file)) {
		error("Error reading READFILE file '%s': %s", name.c_str(), strerror(errno));
		return "";
	}

	return contents;
}

uint32_t act_StringToNum(std::vector<int32_t> const &str) {
	uint32_t length = str.size();

	if (length == 1) {
		// The string is a single character with a single value,
		// which can be used directly as a number.
		return static_cast<uint32_t>(str[0]);
	}

	warning(WARNING_OBSOLETE, "Treating multi-unit strings as numbers is deprecated");

	for (int32_t v : str) {
		if (!checkNBit(v, 8, "All character units")) {
			break;
		}
	}

	uint32_t r = 0;

	for (uint32_t i = length < 4 ? 0 : length - 4; i < length; ++i) {
		r <<= 8;
		r |= static_cast<uint8_t>(str[i]);
	}

	return r;
}

static void errorInvalidUTF8Byte(uint8_t byte, char const *functionName) {
	error("%s: Invalid UTF-8 byte 0x%02hhX", functionName, byte);
}

size_t act_StringLen(std::string const &str, bool printErrors) {
	size_t len = 0;
	uint32_t state = UTF8_ACCEPT;
	uint32_t codepoint = 0;

	for (char c : str) {
		uint8_t byte = static_cast<uint8_t>(c);

		switch (decode(&state, &codepoint, byte)) {
		case UTF8_REJECT:
			if (printErrors) {
				errorInvalidUTF8Byte(byte, "STRLEN");
			}
			state = UTF8_ACCEPT;
			// fallthrough
		case UTF8_ACCEPT:
			++len;
			break;
		}
	}

	// Check for partial code point.
	if (state != UTF8_ACCEPT) {
		if (printErrors) {
			error("STRLEN: Incomplete UTF-8 character");
		}
		++len;
	}

	return len;
}

std::string act_StringSlice(std::string const &str, uint32_t start, uint32_t stop) {
	size_t strLen = str.length();
	size_t index = 0;
	uint32_t state = UTF8_ACCEPT;
	uint32_t codepoint = 0;
	uint32_t curIdx = 0;

	// Advance to starting index in source string.
	while (index < strLen && curIdx < start) {
		switch (decode(&state, &codepoint, str[index])) {
		case UTF8_REJECT:
			errorInvalidUTF8Byte(str[index], "STRSLICE");
			state = UTF8_ACCEPT;
			// fallthrough
		case UTF8_ACCEPT:
			++curIdx;
			break;
		}
		++index;
	}

	// An index 1 past the end of the string is allowed, but will trigger the
	// "Length too big" warning below if the length is nonzero.
	if (index >= strLen && start > curIdx) {
		warning(
		    WARNING_BUILTIN_ARG,
		    "STRSLICE: Start index %" PRIu32 " is past the end of the string",
		    start
		);
	}

	size_t startIndex = index;

	// Advance to ending index in source string.
	while (index < strLen && curIdx < stop) {
		switch (decode(&state, &codepoint, str[index])) {
		case UTF8_REJECT:
			errorInvalidUTF8Byte(str[index], "STRSLICE");
			state = UTF8_ACCEPT;
			// fallthrough
		case UTF8_ACCEPT:
			++curIdx;
			break;
		}
		++index;
	}

	// Check for partial code point.
	if (state != UTF8_ACCEPT) {
		error("STRSLICE: Incomplete UTF-8 character");
		++curIdx;
	}

	if (curIdx < stop) {
		warning(
		    WARNING_BUILTIN_ARG,
		    "STRSLICE: Stop index %" PRIu32 " is past the end of the string",
		    stop
		);
	}

	return str.substr(startIndex, index - startIndex);
}

std::string act_StringSub(std::string const &str, uint32_t pos, uint32_t len) {
	size_t strLen = str.length();
	size_t index = 0;
	uint32_t state = UTF8_ACCEPT;
	uint32_t codepoint = 0;
	uint32_t curPos = 1;

	// Advance to starting position in source string.
	while (index < strLen && curPos < pos) {
		switch (decode(&state, &codepoint, str[index])) {
		case UTF8_REJECT:
			errorInvalidUTF8Byte(str[index], "STRSUB");
			state = UTF8_ACCEPT;
			// fallthrough
		case UTF8_ACCEPT:
			++curPos;
			break;
		}
		++index;
	}

	// A position 1 past the end of the string is allowed, but will trigger the
	// "Length too big" warning below if the length is nonzero.
	if (index >= strLen && pos > curPos) {
		warning(
		    WARNING_BUILTIN_ARG, "STRSUB: Position %" PRIu32 " is past the end of the string", pos
		);
	}

	size_t startIndex = index;
	uint32_t curLen = 0;

	// Compute the result length in bytes.
	while (index < strLen && curLen < len) {
		switch (decode(&state, &codepoint, str[index])) {
		case UTF8_REJECT:
			errorInvalidUTF8Byte(str[index], "STRSUB");
			state = UTF8_ACCEPT;
			// fallthrough
		case UTF8_ACCEPT:
			++curLen;
			break;
		}
		++index;
	}

	// Check for partial code point.
	if (state != UTF8_ACCEPT) {
		error("STRSUB: Incomplete UTF-8 character");
		++curLen;
	}

	if (curLen < len) {
		warning(WARNING_BUILTIN_ARG, "STRSUB: Length too big: %" PRIu32, len);
	}

	return str.substr(startIndex, index - startIndex);
}

size_t act_CharLen(std::string const &str) {
	std::string_view view = str;
	size_t len;

	for (len = 0; charmap_ConvertNext(view, nullptr); ++len) {}

	return len;
}

std::string act_StringChar(std::string const &str, uint32_t idx) {
	std::string_view view = str;
	size_t charLen = 1;

	// Advance to starting index in source string.
	for (uint32_t curIdx = 0; charLen && curIdx < idx; ++curIdx) {
		charLen = charmap_ConvertNext(view, nullptr);
	}

	std::string_view start = view;

	if (!charmap_ConvertNext(view, nullptr)) {
		warning(
		    WARNING_BUILTIN_ARG, "STRCHAR: Index %" PRIu32 " is past the end of the string", idx
		);
	}

	start = start.substr(0, start.length() - view.length());
	return std::string(start);
}

std::string act_CharSub(std::string const &str, uint32_t pos) {
	std::string_view view = str;
	size_t charLen = 1;

	// Advance to starting position in source string.
	for (uint32_t curPos = 1; charLen && curPos < pos; ++curPos) {
		charLen = charmap_ConvertNext(view, nullptr);
	}

	std::string_view start = view;

	if (!charmap_ConvertNext(view, nullptr)) {
		warning(
		    WARNING_BUILTIN_ARG, "CHARSUB: Position %" PRIu32 " is past the end of the string", pos
		);
	}

	start = start.substr(0, start.length() - view.length());
	return std::string(start);
}

int32_t act_CharCmp(std::string_view str1, std::string_view str2) {
	std::vector<int32_t> seq1, seq2;
	size_t idx1 = 0, idx2 = 0;
	for (;;) {
		if (idx1 >= seq1.size()) {
			idx1 = 0;
			seq1.clear();
			charmap_ConvertNext(str1, &seq1);
		}
		if (idx2 >= seq2.size()) {
			idx2 = 0;
			seq2.clear();
			charmap_ConvertNext(str2, &seq2);
		}
		if (seq1.empty() != seq2.empty()) {
			return seq1.empty() ? -1 : 1;
		} else if (seq1.empty()) {
			return 0;
		} else {
			int32_t value1 = seq1[idx1++], value2 = seq2[idx2++];
			if (value1 != value2) {
				return (value1 > value2) - (value1 < value2);
			}
		}
	}
}

uint32_t act_AdjustNegativeIndex(int32_t idx, size_t len, char const *functionName) {
	// String functions adjust negative index arguments the same way,
	// such that position -1 is the last character of a string.
	if (idx < 0) {
		idx += len;
	}
	if (idx < 0) {
		warning(WARNING_BUILTIN_ARG, "%s: Index starts at 0", functionName);
		idx = 0;
	}
	return static_cast<uint32_t>(idx);
}

uint32_t act_AdjustNegativePos(int32_t pos, size_t len, char const *functionName) {
	// STRSUB and CHARSUB adjust negative position arguments the same way,
	// such that position -1 is the last character of a string.
	if (pos < 0) {
		pos += len + 1;
	}
	if (pos < 1) {
		warning(WARNING_BUILTIN_ARG, "%s: Position starts at 1", functionName);
		pos = 1;
	}
	return static_cast<uint32_t>(pos);
}

std::string
    act_StringReplace(std::string_view str, std::string const &old, std::string const &rep) {
	if (old.empty()) {
		warning(WARNING_EMPTY_STRRPL, "STRRPL: Cannot replace an empty string");
		return std::string(str);
	}

	std::string rpl;

	while (!str.empty()) {
		auto pos = str.find(old);
		if (pos == str.npos) {
			rpl.append(str);
			break;
		}
		rpl.append(str, 0, pos);
		rpl.append(rep);
		str.remove_prefix(pos + old.size());
	}

	return rpl;
}

std::string act_StringFormat(
    std::string const &spec, std::vector<std::variant<uint32_t, std::string>> const &args
) {
	std::string str;
	size_t argIndex = 0;

	for (size_t i = 0; spec[i] != '\0'; ++i) {
		int c = spec[i];

		if (c != '%') {
			str += c;
			continue;
		}

		c = spec[++i];

		if (c == '%') {
			str += c;
			continue;
		}

		FormatSpec fmt{};

		while (c != '\0') {
			fmt.useCharacter(c);
			if (fmt.isFinished()) {
				break;
			}
			c = spec[++i];
		}

		if (fmt.isEmpty()) {
			error("STRFMT: Illegal '%%' at end of format string");
			str += '%';
			break;
		}

		if (!fmt.isValid()) {
			error("STRFMT: Invalid format spec for argument %zu", argIndex + 1);
			str += '%';
		} else if (argIndex >= args.size()) {
			// Will warn after formatting is done.
			str += '%';
		} else if (std::holds_alternative<uint32_t>(args[argIndex])) {
			fmt.appendNumber(str, std::get<uint32_t>(args[argIndex]));
		} else {
			fmt.appendString(str, std::get<std::string>(args[argIndex]));
		}

		++argIndex;
	}

	if (argIndex < args.size()) {
		error("STRFMT: %zu unformatted argument(s)", args.size() - argIndex);
	} else if (argIndex > args.size()) {
		error(
		    "STRFMT: Not enough arguments for format spec, got: %zu, need: %zu",
		    args.size(),
		    argIndex
		);
	}

	return str;
}

void act_CompoundAssignment(std::string const &symName, RPNCommand op, int32_t constValue) {
	Expression oldExpr, constExpr, newExpr;
	int32_t newValue;

	oldExpr.makeSymbol(symName);
	constExpr.makeNumber(constValue);
	newExpr.makeBinaryOp(op, std::move(oldExpr), constExpr);
	newValue = newExpr.getConstVal();
	sym_AddVar(symName, newValue);
}

void act_FailAssert(AssertionType type) {
	switch (type) {
	case ASSERT_FATAL:
		fatal("Assertion failed");
	case ASSERT_ERROR:
		error("Assertion failed");
		break;
	case ASSERT_WARN:
		warning(WARNING_ASSERT, "Assertion failed");
		break;
	}
}

void act_FailAssertMsg(AssertionType type, std::string const &message) {
	switch (type) {
	case ASSERT_FATAL:
		fatal("Assertion failed: %s", message.c_str());
	case ASSERT_ERROR:
		error("Assertion failed: %s", message.c_str());
		break;
	case ASSERT_WARN:
		warning(WARNING_ASSERT, "Assertion failed: %s", message.c_str());
		break;
	}
}
