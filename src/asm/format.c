/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2020, RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asm/format.h"
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

void fmt_PrintString(char *buf, size_t bufLen, struct FormatSpec const *fmt, char const *value)
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

	size_t len = strlen(value);
	size_t totalLen = fmt->width > len ? fmt->width : len;

	if (totalLen + 1 > bufLen) /* bufLen includes terminator */
		error("Formatted string value too long\n");

	size_t padLen = fmt->width > len ? fmt->width - len : 0;

	if (fmt->alignLeft) {
		strncpy(buf, value, len < bufLen ? len : bufLen);
		for (size_t i = 0; i < totalLen && len + i < bufLen; i++)
			buf[len + i] = ' ';
	} else {
		for (size_t i = 0; i < padLen && i < bufLen; i++)
			buf[i] = ' ';
		if (bufLen > padLen)
			strncpy(buf + padLen, value, bufLen - padLen - 1);
	}

	buf[totalLen] = '\0';
}

void fmt_PrintNumber(char *buf, size_t bufLen, struct FormatSpec const *fmt, uint32_t value)
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
		uint8_t fracWidth = fmt->hasFrac ? fmt->fracWidth : 5;

		if (fracWidth) {
			char spec[16]; /* Max "%" + 5-char PRIu32 + ".%0255.f" + terminator */

			snprintf(spec, sizeof(spec), "%%" PRIu32 ".%%0%d.f", fracWidth);
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
	size_t numLen = len;

	if (sign)
		numLen++;
	if (prefix)
		numLen++;

	size_t totalLen = fmt->width > numLen ? fmt->width : numLen;

	if (totalLen + 1 > bufLen) /* bufLen includes terminator */
		error("Formatted numeric value too long\n");

	size_t padLen = fmt->width > numLen ? fmt->width - numLen : 0;

	if (fmt->alignLeft) {
		size_t pos = 0;

		if (sign && pos < bufLen)
			buf[pos++] = sign;
		if (prefix && pos < bufLen)
			buf[pos++] = prefix;

		strcpy(buf + pos, valueBuf);
		pos += len;

		for (size_t i = 0; i < totalLen && pos + i < bufLen; i++)
			buf[pos + i] = ' ';
	} else {
		size_t pos = 0;

		if (fmt->padZero) {
			/* sign, then prefix, then zero padding */
			if (sign && pos < bufLen)
				buf[pos++] = sign;
			if (prefix && pos < bufLen)
				buf[pos++] = prefix;
			for (size_t i = 0; i < padLen && pos < bufLen; i++)
				buf[pos++] = '0';
		} else {
			/* space padding, then sign, then prefix */
			for (size_t i = 0; i < padLen && pos < bufLen; i++)
				buf[pos++] = ' ';
			if (sign && pos < bufLen)
				buf[pos++] = sign;
			if (prefix && pos < bufLen)
				buf[pos++] = prefix;
		}
		if (bufLen > pos)
			strcpy(buf + pos, valueBuf);
	}

	buf[totalLen] = '\0';
}
