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

void act_If(int32_t condition) {
	lexer_IncIFDepth();

	if (condition) {
		lexer_RunIFBlock();
	} else {
		lexer_SetMode(LEXER_SKIP_TO_ELIF);
	}
}

void act_Elif(int32_t condition) {
	if (lexer_GetIFDepth() == 0) {
		fatal("Found ELIF outside of an IF construct");
	}
	if (lexer_RanIFBlock()) {
		if (lexer_ReachedELSEBlock()) {
			fatal("Found ELIF after an ELSE block");
		}
		lexer_SetMode(LEXER_SKIP_TO_ENDC);
	} else if (condition) {
		lexer_RunIFBlock();
	} else {
		lexer_SetMode(LEXER_SKIP_TO_ELIF);
	}
}

void act_Else() {
	if (lexer_GetIFDepth() == 0) {
		fatal("Found ELSE outside of an IF construct");
	}
	if (lexer_RanIFBlock()) {
		if (lexer_ReachedELSEBlock()) {
			fatal("Found ELSE after an ELSE block");
		}
		lexer_SetMode(LEXER_SKIP_TO_ENDC);
	} else {
		lexer_RunIFBlock();
		lexer_ReachELSEBlock();
	}
}

void act_Endc() {
	lexer_DecIFDepth();
}

AlignmentSpec act_Alignment(int32_t alignment, int32_t alignOfs) {
	AlignmentSpec spec = {0, 0};
	if (alignment > 16) {
		error("Alignment must be between 0 and 16, not %u", alignment);
	} else if (alignOfs <= -(1 << alignment) || alignOfs >= 1 << alignment) {
		error(
		    "The absolute alignment offset (%" PRIu32 ") must be less than alignment size (%d)",
		    static_cast<uint32_t>(alignOfs < 0 ? -alignOfs : alignOfs),
		    1 << alignment
		);
	} else {
		spec.alignment = alignment;
		spec.alignOfs = alignOfs < 0 ? (1 << alignment) + alignOfs : alignOfs;
	}
	return spec;
}

static void failAssert(AssertionType type, std::string const &message) {
	switch (type) {
	case ASSERT_FATAL:
		if (message.empty()) {
			fatal("Assertion failed");
		} else {
			fatal("Assertion failed: %s", message.c_str());
		}
	case ASSERT_ERROR:
		if (message.empty()) {
			error("Assertion failed");
		} else {
			error("Assertion failed: %s", message.c_str());
		}
		break;
	case ASSERT_WARN:
		if (message.empty()) {
			warning(WARNING_ASSERT, "Assertion failed");
		} else {
			warning(WARNING_ASSERT, "Assertion failed: %s", message.c_str());
		}
		break;
	}
}

void act_Assert(AssertionType type, Expression const &expr, std::string const &message) {
	if (!expr.isKnown()) {
		out_CreateAssert(type, expr, message, sect_GetOutputOffset());
	} else if (expr.value() == 0) {
		failAssert(type, message);
	}
}

void act_StaticAssert(AssertionType type, int32_t condition, std::string const &message) {
	if (!condition) {
		failAssert(type, message);
	}
}

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

uint32_t act_CharToNum(std::string const &str) {
	if (std::vector<int32_t> output = charmap_Convert(str); output.size() == 1) {
		return static_cast<uint32_t>(output[0]);
	} else {
		error("Character literals must be a single charmap unit");
		return 0;
	}
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

static uint32_t adjustNegativeIndex(int32_t idx, size_t len, char const *functionName) {
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

static uint32_t adjustNegativePos(int32_t pos, size_t len, char const *functionName) {
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

int32_t act_CharVal(std::string const &str, int32_t negIdx) {
	if (size_t len = charmap_CharSize(str); len != 0) {
		uint32_t idx = adjustNegativeIndex(negIdx, len, "CHARVAL");
		if (std::optional<int32_t> val = charmap_CharValue(str, idx); val.has_value()) {
			return *val;
		} else {
			warning(
			    WARNING_BUILTIN_ARG,
			    "CHARVAL: Index %" PRIu32 " is past the end of the character mapping",
			    idx
			);
			return 0;
		}
	} else {
		error("CHARVAL: No character mapping for \"%s\"", str.c_str());
		return 0;
	}
}

uint8_t act_StringByte(std::string const &str, int32_t negIdx) {
	size_t len = str.length();
	if (uint32_t idx = adjustNegativeIndex(negIdx, len, "STRBYTE"); idx < len) {
		return static_cast<uint8_t>(str[idx]);
	} else {
		warning(
		    WARNING_BUILTIN_ARG, "STRBYTE: Index %" PRIu32 " is past the end of the string", idx
		);
		return 0;
	}
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

std::string
    act_StringSlice(std::string const &str, int32_t negStart, std::optional<int32_t> negStop) {
	size_t adjustLen = act_StringLen(str, false);
	uint32_t start = adjustNegativeIndex(negStart, adjustLen, "STRSLICE");
	uint32_t stop = negStop ? adjustNegativeIndex(*negStop, adjustLen, "STRSLICE") : adjustLen;

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

std::string act_StringSub(std::string const &str, int32_t negPos, std::optional<uint32_t> optLen) {
	size_t adjustLen = act_StringLen(str, false);
	uint32_t pos = adjustNegativePos(negPos, adjustLen, "STRSUB");
	uint32_t len = optLen ? *optLen : pos > adjustLen ? 0 : adjustLen + 1 - pos;

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

std::string act_StringChar(std::string const &str, int32_t negIdx) {
	size_t adjustLen = act_CharLen(str);
	uint32_t idx = adjustNegativeIndex(negIdx, adjustLen, "STRCHAR");

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

std::string act_CharSub(std::string const &str, int32_t negPos) {
	size_t adjustLen = act_CharLen(str);
	uint32_t pos = adjustNegativePos(negPos, adjustLen, "CHARSUB");

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

std::string act_SectionName(std::string const &symName) {
	Symbol *sym = sym_FindScopedValidSymbol(symName);
	if (!sym) {
		if (sym_IsPurgedScoped(symName)) {
			fatal("Unknown symbol \"%s\"; it was purged", symName.c_str());
		} else {
			fatal("Unknown symbol \"%s\"", symName.c_str());
		}
	}

	Section const *section = sym->getSection();
	if (!section) {
		fatal("\"%s\" does not belong to any section", sym->name.c_str());
	}

	return section->name;
}

void act_CompoundAssignment(std::string const &symName, RPNCommand op, int32_t constValue) {
	Expression oldExpr, constExpr, newExpr;
	oldExpr.makeSymbol(symName);
	constExpr.makeNumber(constValue);
	newExpr.makeBinaryOp(op, std::move(oldExpr), constExpr);

	int32_t newValue = newExpr.getConstVal();
	sym_AddVar(symName, newValue);
}
