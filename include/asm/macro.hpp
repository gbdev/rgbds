/* SPDX-License-Identifier: MIT */

#ifndef RGBDS_ASM_MACRO_HPP
#define RGBDS_ASM_MACRO_HPP

#include <memory>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

struct MacroArgs {
	unsigned int shift;
	std::vector<std::pair<std::shared_ptr<std::string>, bool>> args;

	uint32_t nbArgs() const { return args.size() - shift; }
	std::shared_ptr<std::string> getArg(uint32_t i);
	std::shared_ptr<std::string> getAllArgs();

	void appendArg(std::shared_ptr<std::string> arg);
	void shiftArgs(int32_t count);

	void checkUsedArgs() const;
};

#endif // RGBDS_ASM_MACRO_HPP
