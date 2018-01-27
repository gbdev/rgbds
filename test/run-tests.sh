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

git clone https://github.com/pret/pokecrystal.git --depth=1
pushd pokecrystal
make -j
make compare
popd

git clone --recursive https://github.com/pret/pokered.git --depth=1
pushd pokered
make -j
make compare
popd

git clone https://github.com/AntonioND/ucity.git --depth=1
pushd ucity
make -j
popd
