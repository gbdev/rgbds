/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_MACRO_H
#define RGBDS_MACRO_H

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

struct MacroArgs {
	unsigned int shift;
	std::vector<std::shared_ptr<std::string>> args;

	void append(std::shared_ptr<std::string> arg);
};

MacroArgs *macro_GetCurrentArgs();
void macro_UseNewArgs(MacroArgs *args);
std::shared_ptr<std::string> macro_GetArg(uint32_t i);
std::shared_ptr<std::string> macro_GetAllArgs();

void macro_ShiftCurrentArgs(int32_t count);
uint32_t macro_NbArgs();

#endif // RGBDS_MACRO_H
