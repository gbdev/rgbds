// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_LAYOUT_HPP
#define RGBDS_LINK_LAYOUT_HPP

#include <stdint.h>
#include <string>

#include "linkdefs.hpp"

void layout_SetFloatingSectionType(SectionType type);
void layout_SetSectionType(SectionType type);
void layout_SetSectionType(SectionType type, uint32_t bank);

void layout_SetAddr(uint32_t addr);
void layout_MakeAddrFloating();
void layout_AlignTo(uint32_t alignment, uint32_t offset);
void layout_Pad(uint32_t length);

void layout_PlaceSection(std::string const &name, bool isOptional);

#endif // RGBDS_LINK_LAYOUT_HPP
