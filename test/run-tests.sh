#!/usr/bin/env bash
set -euo pipefail

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
			echo "$(basename "$0"): internal error"
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

test_downstream() { # owner repo make-target build-file build-hash
	if ! pushd "$2"; then
		echo >&2 'Please run `'"$FETCH_TEST_DEPS"'` before running the test suite'
		return 1
	fi
	make clean
	make -j4 "$3" RGBDS=../../
	hash="$(sha1sum -b "$4" | head -c 40)"
	if [ "$hash" != "$5" ]; then
		echo >&2 'SHA-1 hash of '"$4"' did not match: '"$hash"
		return 1
	fi
	popd
}

if "$nonfree"; then
	test_downstream pret  pokecrystal      compare pokecrystal.gbc f4cd194bdee0d04ca4eac29e09b8e4e9d818c133
	test_downstream pret  pokered          compare pokered.gbc     ea9bcae617fdf159b045185467ae58b2e4a48b9a
	test_downstream zladx LADX-Disassembly default azle.gbc        d90ac17e9bf17b6c61624ad9f05447bdb5efc01a
fi
test_downstream AntonioND ucity   all      ucity.gbc 67e67a4eaefb12ddc98d3caa8959102b885c0405
test_downstream pinobatch libbet  all      libbet.gb 8bf954a61cd31fa665ad0abb7cea14561919a12c
test_downstream LIJI32    SameBoy bootroms build/bin/BootROMs/cgb_boot.bin 113903775a9d34b798c2f8076672da6626815a91
