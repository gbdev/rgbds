#!/usr/bin/env bash

# SPDX-License-Identifier: MIT

declare -A FILES
while read -r -d '' file; do
	FILES["$file"]="true"
done < <(git diff --name-only -z "$1" HEAD)

edited () {
	${FILES["$1"]:-"false"}
}

dependency () {
	if edited "$1" && ! edited "$2"; then
		echo "'$1' was modified, but not '$2'! $3" | xargs
	fi
}

# Pull requests that edit the first file without the second may be correct,
# but are suspicious enough to require review.

dependency include/linkdefs.hpp    man/rgbds.5 \
           "Was the object file format changed?"

dependency src/asm/parser.y        man/rgbasm.5 \
           "Was the rgbasm grammar changed?"

dependency src/link/script.y       man/rgblink.5 \
           "Was the linker script grammar changed?"

dependency include/asm/warning.hpp man/rgbasm.1 \
           "Were the rgbasm warnings changed?"

dependency src/asm/object.cpp      include/linkdefs.hpp \
           "Should the object file revision be bumped?"
dependency src/link/object.cpp     include/linkdefs.hpp \
           "Should the object file revision be bumped?"

dependency Makefile                CMakeLists.txt \
           "Did the build process change?"
dependency Makefile                src/CMakeLists.txt \
           "Did the build process change?"

dependency src/asm/main.cpp        man/rgbasm.1 \
           "Did the rgbasm CLI change?"
dependency src/asm/main.cpp        contrib/zsh_compl/_rgbasm \
           "Did the rgbasm CLI change?"
dependency src/asm/main.cpp        contrib/bash_compl/_rgbasm.bash \
           "Did the rgbasm CLI change?"
dependency src/link/main.cpp       man/rgblink.1 \
           "Did the rgblink CLI change?"
dependency src/link/main.cpp       contrib/zsh_compl/_rgblink \
           "Did the rgblink CLI change?"
dependency src/link/main.cpp        contrib/bash_compl/_rgblink.bash \
           "Did the rgblink CLI change?"
dependency src/fix/main.cpp        man/rgbfix.1 \
           "Did the rgbfix CLI change?"
dependency src/fix/main.cpp        contrib/zsh_compl/_rgbfix \
           "Did the rgbfix CLI change?"
dependency src/fix/main.cpp        contrib/bash_compl/_rgbfix.bash \
           "Did the rgbfix CLI change?"
dependency src/gfx/main.cpp        man/rgbgfx.1 \
           "Did the rgbgfx CLI change?"
dependency src/gfx/main.cpp        contrib/zsh_compl/_rgbgfx \
           "Did the rgbgfx CLI change?"
dependency src/gfx/main.cpp        contrib/bash_compl/_rgbgfx.bash \
           "Did the rgbgfx CLI change?"

dependency test/fetch-test-deps.sh CONTRIBUTING.md \
           "Did the test protocol change?"
dependency test/run-tests.sh       CONTRIBUTING.md \
           "Did the test protocol change?"
dependency test/asm/test.sh        CONTRIBUTING.md \
           "Did the RGBASM test protocol change?"
dependency test/link/test.sh       CONTRIBUTING.md \
           "Did the RGBLINK test protocol change?"
dependency test/fix/test.sh        CONTRIBUTING.md \
           "Did the RGBFIX test protocol change?"
dependency test/gfx/test.sh        CONTRIBUTING.md \
           "Did the RGBGFX test protocol change?"
