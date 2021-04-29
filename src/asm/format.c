/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/format.h"
#include "asm/string.h"
#include "asm/warning.h"

struct FormatSpec fmt_NewSpec(void)
{
	struct FormatSpec fmt = {0};

	return fmt;
}

bool fmt_IsEmpty(struct FormatSpec const *fmt)
{
	return !fmt->state;
}

bool fmt_IsValid(struct FormatSpec const *fmt)
{
	return fmt->valid || fmt->state == FORMAT_DONE;
}

bool fmt_IsFinished(struct FormatSpec const *fmt)
{
	return fmt->state >= FORMAT_DONE;
}

void fmt_UseCharacter(struct FormatSpec *fmt, int c)
{
	if (fmt->state == FORMAT_INVALID)
		return;

	switch (c) {
	/* sign */
	case ' ':
	case '+':
		if (fmt->state > FORMAT_SIGN)
			goto invalid;
		fmt->state = FORMAT_PREFIX;
		fmt->sign = c;
		break;

	/* prefix */
	case '#':
		if (fmt->state > FORMAT_PREFIX)
			goto invalid;
		fmt->state = FORMAT_ALIGN;
		fmt->prefix = true;
		break;

	/* align */
	case '-':
		if (fmt->state > FORMAT_ALIGN)
			goto invalid;
		fmt->state = FORMAT_WIDTH;
		fmt->alignLeft = true;
		break;

	/* pad and width */
	case '0':
		if (fmt->state < FORMAT_WIDTH)
			fmt->padZero = true;
		/* fallthrough */
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		if (fmt->state < FORMAT_WIDTH) {
			fmt->state = FORMAT_WIDTH;
			fmt->width = c - '0';
		} else if (fmt->state == FORMAT_WIDTH) {
			fmt->width = fmt->width * 10 + (c - '0');
		} else if (fmt->state == FORMAT_FRAC) {
			fmt->fracWidth = fmt->fracWidth * 10 + (c - '0');
		} else {
			goto invalid;
		}
		break;

	case '.':
		if (fmt->state > FORMAT_WIDTH)
			goto invalid;
		fmt->state = FORMAT_FRAC;
		fmt->hasFrac = true;
		break;

	/* type */
	case 'd':
	case 'u':
	case 'X':
	case 'x':
	case 'b':
	case 'o':
	case 'f':
	case 's':
		if (fmt->state >= FORMAT_DONE)
			goto invalid;
		fmt->state = FORMAT_DONE;
		fmt->valid = true;
		fmt->type = c;
		break;

	default:
invalid:
		fmt->state = FORMAT_INVALID;
		fmt->valid = false;
	}
}

void fmt_FinishCharacters(struct FormatSpec *fmt)
{
	if (!fmt_IsValid(fmt))
		fmt->state = FORMAT_INVALID;
}

struct String *fmt_PrintString(struct FormatSpec const *fmt, struct String const *value)
{
	if (fmt->sign)
		error("Formatting string with sign flag '%c'\n", fmt->sign);
	if (fmt->prefix)
		error("Formatting string with prefix flag '#'\n");
	if (fmt->padZero)
		error("Formatting string with padding flag '0'\n");
	if (fmt->hasFrac)
		error("Formatting string with fractional width\n");
	if (fmt->type != 's')
		error("Formatting string as type '%c'\n", fmt->type);

	size_t len = str_Len(value);
	size_t totalLen = fmt->width > len ? fmt->width : len;
	size_t padLen = totalLen - len;
	struct String *str = str_New(totalLen);

	if (!str)
		return NULL;

	if (fmt->alignLeft) {
		MUTATE_STR(str, str_Append(str, value));
		for (size_t i = 0; i < padLen; i++)
			MUTATE_STR(str, str_Push(str, ' '));
	} else {
		for (size_t i = 0; i < padLen; i++)
			MUTATE_STR(str, str_Push(str, ' '));
		MUTATE_STR(str, str_Append(str, value));
	}

	return str;
}

struct String *fmt_PrintNumber(struct FormatSpec const *fmt, uint32_t value)
{
	if (fmt->type != 'X' && fmt->type != 'x' && fmt->type != 'b' && fmt->type != 'o'
	    && fmt->prefix)
		error("Formatting type '%c' with prefix flag '#'\n", fmt->type);
	if (fmt->type != 'f' && fmt->hasFrac)
		error("Formatting type '%c' with fractional width\n", fmt->type);
	if (fmt->type == 's')
		error("Formatting number as type 's'\n");

	char sign = fmt->sign; /* 0 or ' ' or '+' */

	if (fmt->type == 'd' || fmt->type == 'f') {
		int32_t v = value;

		if (v < 0 && v != INT32_MIN) {
			sign = '-';
			value = -v;
		}
	}

	char prefix = !fmt->prefix ? 0
		: fmt->type == 'X' ? '$'
		: fmt->type == 'x' ? '$'
		: fmt->type == 'b' ? '%'
		: fmt->type == 'o' ? '&'
		: 0;

	char valueBuf[262]; /* Max 5 digits + decimal + 255 fraction digits + terminator */

	if (fmt->type == 'b') {
		/* Special case for binary */
		char *ptr = valueBuf;

		do {
			*ptr++ = (value & 1) + '0';
			value >>= 1;
		} while (value);

		*ptr = '\0';

		/* Reverse the digits */
		size_t valueLen = ptr - valueBuf;

		for (size_t i = 0, j = valueLen - 1; i < j; i++, j--) {
			char c = valueBuf[i];

			valueBuf[i] = valueBuf[j];
			valueBuf[j] = c;
		}
	} else if (fmt->type == 'f') {
		/* Special case for fixed-point */

		/* Default fractional width (C's is 6 for "%f"; here 5 is enough) */
		size_t fracWidth = fmt->hasFrac ? fmt->fracWidth : 5;

		if (fracWidth) {
			if (fracWidth > 255) {
				error("Fractional width %zu too long, limiting to 255\n",
				      fracWidth);
				fracWidth = 255;
			}

			char spec[16]; /* Max "%" + 5-char PRIu32 + ".%0255.f" + terminator */

			snprintf(spec, sizeof(spec), "%%" PRIu32 ".%%0%zu.f", fracWidth);
			snprintf(valueBuf, sizeof(valueBuf), spec, value >> 16,
				 (value % 65536) / 65536.0 * pow(10, fracWidth) + 0.5);
		} else {
			snprintf(valueBuf, sizeof(valueBuf), "%" PRIu32, value >> 16);
		}
	} else {
		char const *spec = fmt->type == 'd' ? "%" PRId32
				 : fmt->type == 'u' ? "%" PRIu32
				 : fmt->type == 'X' ? "%" PRIX32
				 : fmt->type == 'x' ? "%" PRIx32
				 : fmt->type == 'o' ? "%" PRIo32
				 : "%" PRId32;

		snprintf(valueBuf, sizeof(valueBuf), spec, value);
	}

	size_t len = strlen(valueBuf);
	size_t numLen = !!sign + !!prefix + len;
	size_t totalLen = fmt->width > numLen ? fmt->width : numLen;
	size_t padLen = totalLen - numLen;
	struct String *str = str_New(totalLen);

	if (!str)
		return NULL;

	if (fmt->alignLeft) {
		if (sign)
			MUTATE_STR(str, str_Push(str, sign));
		if (prefix)
			MUTATE_STR(str, str_Push(str, prefix));
		MUTATE_STR(str, str_AppendSlice(str, valueBuf, len));
		for (size_t i = 0; i < padLen; i++)
			MUTATE_STR(str, str_Push(str, ' '));
	} else {
		if (fmt->padZero) {
			/* sign, then prefix, then zero padding */
			if (sign)
				MUTATE_STR(str, str_Push(str, sign));
			if (prefix)
				MUTATE_STR(str, str_Push(str, prefix));
			for (size_t i = 0; i < padLen; i++)
				MUTATE_STR(str, str_Push(str, '0'));
		} else {
			/* space padding, then sign, then prefix */
			for (size_t i = 0; i < padLen; i++)
				MUTATE_STR(str, str_Push(str, ' '));
			if (sign)
				MUTATE_STR(str, str_Push(str, sign));
			if (prefix)
				MUTATE_STR(str, str_Push(str, prefix));
		}
		MUTATE_STR(str, str_AppendSlice(str, valueBuf, len));
	}

	return str;
}
