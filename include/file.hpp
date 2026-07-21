// SPDX-License-Identifier: MIT

#ifndef RGBDS_FILE_HPP
#define RGBDS_FILE_HPP

#include <fcntl.h>
#include <fstream>
#include <ios>
#include <iostream>
#include <streambuf>
#include <string>
#include <variant>

#include "helpers.hpp" // assume
#include "platform.hpp"

class File {
	std::variant<std::streambuf *, std::filebuf> _file;

public:
	File() : _file(nullptr) {}

	// This should only be called once, and before doing any `->` operations.
	// Returns `nullptr` on error, and a non-null pointer otherwise.
	File *open(std::string const &path, std::ios_base::openmode mode) {
		if (path != "-") {
			return _file.emplace<std::filebuf>().open(path, mode) ? this : nullptr;
		} else if (mode & std::ios_base::in) {
			assume(!(mode & std::ios_base::out));
			_file.emplace<std::streambuf *>(std::cin.rdbuf());
			if (setmode(STDIN_FILENO, (mode & std::ios_base::binary) ? O_BINARY : O_TEXT) == -1) {
				return nullptr;
			}
		} else {
			assume(mode & std::ios_base::out);
			_file.emplace<std::streambuf *>(std::cout.rdbuf());
			if (setmode(STDOUT_FILENO, (mode & std::ios_base::binary) ? O_BINARY : O_TEXT) == -1) {
				return nullptr;
			}
		}
		return this;
	}
	std::streambuf &operator*() {
		return std::holds_alternative<std::filebuf>(_file) ? std::get<std::filebuf>(_file)
		                                                   : *std::get<std::streambuf *>(_file);
	}
	std::streambuf *operator->() { return &**this; }

	char const *c_str(std::string const &path) const {
		return std::holds_alternative<std::filebuf>(_file)             ? path.c_str()
		       : std::get<std::streambuf *>(_file) == std::cin.rdbuf() ? "<stdin>"
		                                                               : "<stdout>";
	}
};

#endif // RGBDS_FILE_HPP
