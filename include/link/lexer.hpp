// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_LEXER_HPP
#define RGBDS_LINK_LEXER_HPP

#include <stdarg.h>
#include <string>

[[gnu::format(printf, 1, 2)]]
void lexer_Error(char const *fmt, ...);

void lexer_IncludeFile(std::string &&path);
void lexer_IncLineNo();

bool lexer_Init(char const *linkerScriptName);

#endif // RGBDS_LINK_LEXER_HPP
