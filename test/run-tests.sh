#!/bin/bash

# Return failure as soon as a command fails to execute

set -e

# Tests included with the repository

pushd asm
./test.sh
popd

pushd link
./test.sh
popd

# Test some significant external projects that use RGBDS
# When adding new ones, don't forget to add them to the .gitignore!

if [ ! -d pokecrystal ]; then
	git clone https://github.com/pret/pokecrystal.git --shallow-since=2018-06-04 --single-branch
fi
pushd pokecrystal
git fetch
git checkout 06e169d
make -j
make compare
popd

if [ ! -d pokered ]; then
	git clone --recursive https://github.com/pret/pokered.git --shallow-since=2018-03-23 --single-branch
fi
pushd pokered
git fetch
git checkout 98f09b6
make -j
make compare
popd

if [ ! -d ucity ]; then
	git clone https://github.com/AntonioND/ucity.git --shallow-since=2018-06-05 --single-branch
fi
pushd ucity
git fetch
git checkout 9fc8f27
make -j
popd
