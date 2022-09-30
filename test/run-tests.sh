#!/usr/bin/env bash

# Return failure as soon as a command fails to execute

set -e

cd "$(dirname "$0")"

# Refuse to run if RGBDS isn't present
if [[ ! ( -x ../rgbasm && -x ../rgblink && -x ../rgbfix && -x ../rgbgfx ) ]]; then
	echo "Please build RGBDS before running the tests"
	false
fi

# Tests included with the repository

for dir in asm link fix gfx; do
	pushd $dir
	./test.sh
	popd
done

# Test some significant external projects that use RGBDS
# When adding new ones, don't forget to add them to the .gitignore!
# When updating subprojects, change the commit being checked out, and set the `shallow-since`
# to the day before, to reduce the amount of refs being transferred and thus speed up CI.

test_downstream() { # owner/repo shallow-since commit make-target
	if [ ! -d ${1##*/} ]; then
		git clone https://github.com/$1.git --shallow-since=$2 --single-branch
	fi
	pushd ${1##*/}
	git checkout -f $3
	if [ -f ../patches/${1##*/}.patch ]; then
		git apply --ignore-whitespace ../patches/${1##*/}.patch
	fi
	make clean
	make -j4 $4 RGBDS=../../
	popd
}

test_downstream pret/pokecrystal 2022-09-26 a9869f18962353b056559dc14dfc00fef0df5978 compare
test_downstream pret/pokered     2022-09-25 22859c4bb70dba17994c9b47b07f657ea082875d compare
test_downstream AntonioND/ucity  2022-04-20 d8878233da7a6569f09f87b144cb5bf140146a0f ''
