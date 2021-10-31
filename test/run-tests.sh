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

for dir in asm link fix; do
	pushd $dir
	./test.sh
	popd
done

# Test some significant external projects that use RGBDS
# When adding new ones, don't forget to add them to the .gitignore!

if [ ! -d pokecrystal ]; then
	git clone https://github.com/pret/pokecrystal.git --shallow-since=2021-04-01 --single-branch
fi
pushd pokecrystal
git fetch
git checkout b8fc67848e1d5911204fa42bbd9b954fdec6228a
make clean
make -j4 compare RGBDS=../../
popd

if [ ! -d pokered ]; then
	git clone --recursive https://github.com/pret/pokered.git --shallow-since=2021-04-01 --single-branch
fi
pushd pokered
git fetch
git checkout 0af787ea6d42d6f9c16f952b46519ab94f356abb
make clean
make -j4 compare RGBDS=../../
popd

if [ ! -d ucity ]; then
	git clone https://github.com/AntonioND/ucity.git --shallow-since=2020-11-01 --single-branch
fi
pushd ucity
git fetch
git checkout 15be184b26b337110e1ec2998cd42f134f00f281
make clean
make -j4 RGBDS=../../
popd
