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

if [ ! -d pokecrystal ]; then
	git clone https://github.com/pret/pokecrystal.git --shallow-since=2022-03-12 --single-branch
fi
pushd pokecrystal
git fetch
git checkout a3e31d6463e6313aed12ebc733b3f772f2fc78d7
make clean
make -j4 compare RGBDS=../../
popd

if [ ! -d pokered ]; then
	git clone https://github.com/pret/pokered.git --shallow-since=2022-03-07 --single-branch
fi
pushd pokered
git fetch
git checkout a75dd222709c92ae136d835ff2451391d5a88e45
make clean
make -j4 compare RGBDS=../../
popd

if [ ! -d ucity ]; then
	git clone https://github.com/AntonioND/ucity.git --shallow-since=2020-11-01 --single-branch
fi
pushd ucity
git fetch
git checkout d8878233da7a6569f09f87b144cb5bf140146a0f
make clean
make -j4 RGBDS=../../
popd
