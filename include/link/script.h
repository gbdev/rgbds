/*
 * This file is part of RGBDS.
 *
 * Copyright (c) 2017-2018, Antonio Nino Diaz and RGBDS contributors.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef RGBDS_LINK_SCRIPT_H
#define RGBDS_LINK_SCRIPT_H

#include <stdint.h>

#include "helpers.h"

noreturn_ void script_fatalerror(const char *fmt, ...);

void script_Parse(const char *path);

void script_IncludeFile(const char *path);
int32_t script_IncludeDepthGet(void);
void script_IncludePop(void);

void script_InitSections(void);
void script_SetCurrentSectionType(const char *type, uint32_t bank);
void script_SetAddress(uint32_t addr);
void script_SetAlignment(uint32_t alignment);
void script_OutputSection(const char *section_name);

#endif /* RGBDS_LINK_SCRIPT_H */
