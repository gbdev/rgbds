// SPDX-License-Identifier: MIT

#ifndef RGBDS_LINK_LEXER_HPP
#define RGBDS_LINK_LEXER_HPP

#include <string>

void lexer_TraceCurrent();

void lexer_IncludeFile(std::string &&path);
void lexer_IncLineNo();

bool lexer_Init(std::string const &linkerScriptName);

#endif // RGBDS_LINK_LEXER_HPP
