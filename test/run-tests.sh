#!/bin/bash

# Return failure as soon as a command fails to execute

set -e

cd "$(dirname "$0")"

# Tests included with the repository

pushd asm
./test.sh
popd

pushd link
./test.sh
popd

# Test some significant external projects that use RGBDS
# When adding new ones, don't forget to add them to the .gitignore!

export PATH="$PWD/..:$PATH"

if [ ! -d pokecrystal ]; then
	git clone https://github.com/pret/pokecrystal.git --shallow-since=2018-06-04 --single-branch
fi
pushd pokecrystal
git fetch
git checkout b577e4e179711e96f8e059b42c7115e7103a4a69
make clean
make -j4 compare
popd

if [ ! -d pokered ]; then
	git clone --recursive https://github.com/pret/pokered.git --shallow-since=2018-03-23 --single-branch
fi
pushd pokered
git fetch
git checkout 2fe1505babaf02b995cc0d9c3a827b798935b19a
make clean
make -j4 compare
popd

if [ ! -d ucity ]; then
	git clone https://github.com/AntonioND/ucity.git --shallow-since=2017-07-13 --single-branch
fi
pushd ucity
git fetch
git checkout 780b951b6959b0a98e915ecb9f5fc82544d56d01
make clean
make -j4
popd
