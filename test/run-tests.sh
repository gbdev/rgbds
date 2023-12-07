#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

usage() {
	echo "Runs regression tests on RGBDS."
	echo "Options:"
	echo "    -h, --help      show this help message"
	echo "    --only-free     skip tests that build nonfree codebases"
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
FETCH_TEST_DEPS="fetch-test-deps.sh"
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-free)
			nonfree=false
			FETCH_TEST_DEPS="fetch-test-deps.sh --only-free"
			;;
		--)
			break
			;;
		*)
			echo "$(basename $0): internal error"
			exit 1
			;;
	esac
	shift
done

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

test_downstream() { # owner/repo make-target
	if ! pushd ${1##*/}; then
		echo >&2 'Please run `'"$FETCH_TEST_DEPS"'` before running the test suite'
		return 1
	fi
	make clean
	make -j4 $2 RGBDS=../../
	popd
}

if "$nonfree"; then
	test_downstream pret/pokecrystal       compare
	test_downstream pret/pokered           compare
	test_downstream zladx/LADX-Disassembly ''
fi
test_downstream AntonioND/ucity  ''
test_downstream pinobatch/libbet all
test_downstream LIJI32/SameBoy   bootroms
