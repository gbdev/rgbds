/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_FORMAT_SPEC_H
#define RGBDS_FORMAT_SPEC_H

#include <stddef.h>
#include <stdint.h>
#include <string>

enum FormatState {
	FORMAT_SIGN,    // expects '+' or ' ' (optional)
	FORMAT_PREFIX,  // expects '#' (optional)
	FORMAT_ALIGN,   // expects '-' (optional)
	FORMAT_WIDTH,   // expects '0'-'9', max 255 (optional) (leading '0' indicates pad)
	FORMAT_FRAC,    // got '.', expects '0'-'9', max 255 (optional)
	FORMAT_DONE,    // got [duXxbofs] (required)
	FORMAT_INVALID, // got unexpected character
};

class FormatSpec {
	FormatState state;
	int sign;
	bool prefix;
	bool alignLeft;
	bool padZero;
	size_t width;
	bool hasFrac;
	size_t fracWidth;
	int type;
	bool valid;

public:
	bool isEmpty() const { return !state; }
	bool isValid() const { return valid || state == FORMAT_DONE; }
	bool isFinished() const { return state >= FORMAT_DONE; }

	void useCharacter(int c);
	void finishCharacters();

	std::string formatString(std::string const &value) const;
	std::string formatNumber(uint32_t value) const;
};

#endif // RGBDS_FORMAT_SPEC_H
