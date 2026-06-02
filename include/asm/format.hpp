// SPDX-License-Identifier: MIT

#ifndef RGBDS_ASM_FORMAT_HPP
#define RGBDS_ASM_FORMAT_HPP

#include <stddef.h>
#include <stdint.h>
#include <string>

class FormatSpec {
	size_t width;
	size_t fracWidth;
	size_t precision;
	int sign;
	int type;
	bool exact;
	bool alignLeft;
	bool padZero;
	bool hasFrac;
	bool hasPrec;
	bool parsed;

public:
	bool isValid() const { return !!type; }
	bool isParsed() const { return parsed; }

	size_t parseSpec(char const *spec);

	void appendString(std::string &str, std::string const &value) const;
	void appendNumber(std::string &str, uint32_t value) const;
};

#endif // RGBDS_ASM_FORMAT_HPP
