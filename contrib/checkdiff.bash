#!/bin/bash

# SPDX-License-Identifier: MIT
#
# Copyright (c) 2021 Rangi
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

declare -A FILES
while read -r -d '' file; do
	FILES["$file"]="true"
done < <(git diff --name-only -z $1 HEAD)

edited () {
	${FILES["$1"]:-"false"}
}

dependency () {
	if edited "$1" && ! edited "$2"; then
		default_msg="'$1' was modified, but not '$2'!"
		shift 2
		echo "$@" "$default_msg"
	fi
}

# Pull requests that edit the first file without the second may be correct,
# but are suspicious enough to require review.
dependency include/linkdefs.h    src/rgbds.5         "Should rgbds(5) be synced with the obj file format changes?"
dependency src/asm/parser.y      src/asm/rgbasm.5    "Should rgbasm(5) be synced with the parser changes?"
dependency include/asm/warning.h src/asm/rgbasm.1    "Should rgbasm(1) be synced with the warning changes?"
dependency src/asm/object.c      include/linkdefs.h  "Should the obj file revision be bumped?"
dependency src/link/object.c     include/linkdefs.h  "Should the obj file revision be bumped?"
