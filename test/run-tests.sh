#!/bin/bash

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
	git clone https://github.com/pret/pokecrystal.git --shallow-since=2018-06-04 --single-branch
fi
pushd pokecrystal
git fetch
git checkout 5db892782adaa5bb5a3e4cd6fa06565764bd1fb0
make clean
make -j4 compare RGBDS=../../
popd

if [ ! -d pokered ]; then
	git clone --recursive https://github.com/pret/pokered.git --shallow-since=2018-03-23 --single-branch
fi
pushd pokered
git fetch
git checkout 21908ba30a8bae5c5e1c86b1164402ec95da0220
make clean
make -j4 compare RGBDS=../../
popd

if [ ! -d ucity ]; then
	git clone https://github.com/AntonioND/ucity.git --shallow-since=2017-07-13 --single-branch
fi
pushd ucity
git fetch
git checkout 15be184b26b337110e1ec2998cd42f134f00f281
make clean
make -j4 RGBDS=../../
popd
