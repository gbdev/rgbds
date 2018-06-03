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
	git clone https://github.com/pret/pokecrystal.git --depth=1
fi
pushd pokecrystal
git pull
make -j
make compare
popd

if [ ! -d pokered ]; then
	git clone --recursive https://github.com/pret/pokered.git --depth=1
fi
pushd pokered
git pull
make -j
make compare
popd

if [ ! -d ucity ]; then
	git clone https://github.com/AntonioND/ucity.git --depth=1
fi
pushd ucity
git pull
make -j
popd
