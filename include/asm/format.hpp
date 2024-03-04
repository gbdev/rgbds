/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_FORMAT_SPEC_H
#define RGBDS_FORMAT_SPEC_H

#include <stddef.h>
#include <stdint.h>

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
	enum FormatState state;
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
	void printString(char *buf, size_t bufLen, char const *value);
	void printNumber(char *buf, size_t bufLen, uint32_t value);
};

#endif // RGBDS_FORMAT_SPEC_H
