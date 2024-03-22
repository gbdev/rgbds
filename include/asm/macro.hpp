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

bool macro_HasCurrentArgs();
std::shared_ptr<MacroArgs> macro_GetCurrentArgs();
void macro_UseNewArgs(std::shared_ptr<MacroArgs> args);
uint32_t macro_NbArgs();
std::shared_ptr<std::string> macro_GetArg(uint32_t i);
std::shared_ptr<std::string> macro_GetAllArgs();
void macro_ShiftCurrentArgs(int32_t count);

#endif // RGBDS_MACRO_H
