/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 1997-2018, Carsten Sorensen and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_ASM_ASSERTIONS_H
#define RGBDS_ASM_ASSERTIONS_H

#include <stdint.h>
#include "asm/lexer.h"

enum AssertSeverity {
	SEV_WARN = 0,
	SEV_FAIL = 1,
};

struct Assert {
	char tzName[MAXSTRLEN + 1];
	char tzFilename[_MAX_PATH + 1];
	uint32_t nLine;
	enum AssertSeverity severity;
};

#endif /* RGBDS_ASM_ASSERTIONS_H */
