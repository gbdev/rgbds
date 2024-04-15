/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_MACRO_HPP
#define RGBDS_ASM_MACRO_HPP

#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

struct MacroArgs {
	unsigned int shift;
	std::vector<std::shared_ptr<std::string>> args;

	uint32_t nbArgs() const { return args.size() - shift; }
	std::shared_ptr<std::string> getArg(uint32_t i) const;
	std::shared_ptr<std::string> getAllArgs() const;

	void appendArg(std::shared_ptr<std::string> arg);
	void shiftArgs(int32_t count);
};

#endif // RGBDS_ASM_MACRO_HPP
