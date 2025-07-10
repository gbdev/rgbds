#!/usr/bin/env bash

# SPDX-License-Identifier: MIT

clang-format --version

find . -type f \( -iname '*.hpp' -o -iname '*.cpp' \) -exec clang-format -i {} +

if ! git diff-index --quiet HEAD --; then
	echo 'Unformatted files:'
	git diff-index --name-only HEAD --
	echo
	git diff HEAD --
	exit 1
fi
