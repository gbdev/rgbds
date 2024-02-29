/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_FORMAT_SPEC_H
#define RGBDS_FORMAT_SPEC_H

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

struct FormatSpec {
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
};

FormatSpec fmt_NewSpec(void);
bool fmt_IsEmpty(FormatSpec const *fmt);
bool fmt_IsValid(FormatSpec const *fmt);
bool fmt_IsFinished(FormatSpec const *fmt);
void fmt_UseCharacter(FormatSpec *fmt, int c);
void fmt_FinishCharacters(FormatSpec *fmt);
void fmt_PrintString(char *buf, size_t bufLen, FormatSpec const *fmt, char const *value);
void fmt_PrintNumber(char *buf, size_t bufLen, FormatSpec const *fmt, uint32_t value);

#endif // RGBDS_FORMAT_SPEC_H
