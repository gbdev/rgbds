#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

cd "$(dirname "$0")"

usage() {
	echo "Runs regression tests on RGBDS."
	echo "Options:"
	echo "    -h, --help            show this help message"
	echo "    --only-internal       only run tests that build local examples"
	echo "    --only-external       only run tests that build external codebases"
	echo "    --only-free           skip tests that build nonfree codebases"
	echo "    --installed-rgbds     use the system installed RGBDS"
	echo "                          (only compatible with external codebases)"
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
internal=true
external=true
installedrgbds=false
osname=
FETCH_TEST_DEPS="fetch-test-deps.sh"
RGBDS_PATH="RGBDS=../../"
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-internal)
			external=false
			FETCH_TEST_DEPS="$FETCH_TEST_DEPS --only-internal"
			;;
		--only-external)
			internal=false
			;;
		--only-free)
			nonfree=false
			FETCH_TEST_DEPS="$FETCH_TEST_DEPS --only-free"
			;;
		--installed-rgbds)
			installedrgbds=true
			RGBDS_PATH=
			;;
		--os)
			shift
			osname="$1"
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

if ! ("$internal" || "$external"); then
	echo "Specifying --only-internal with --only-external is a contradiction"
	false
fi

if "$internal" && "$installedrgbds"; then
	echo "Please specify --only-external with --installed-rgbds"
	echo "(internal tests don't support running with system-installed RGBDS)"
	false
fi

# Refuse to run if RGBDS isn't available
if "$installedrgbds"; then
	is_installed() {
		command -v "$1" >/dev/null 2>&1
	}
	if ! (is_installed rgbasm && is_installed rgblink && is_installed rgbfix && is_installed rgbgfx); then
		echo "Please install RGBDS before running the tests"
		false
	fi
else
	if [[ ! ( -x ../rgbasm && -x ../rgblink && -x ../rgbfix && -x ../rgbgfx ) ]]; then
		echo "Please build RGBDS before running the tests"
		false
	fi
fi

# Tests included with the repository
if "$internal"; then
	for dir in asm link fix gfx; do
		pushd $dir
		./test.sh
		popd
	done
fi

if ! "$external"; then
	exit
fi

# Test some significant external projects that use RGBDS
# When adding new ones, don't forget to add them to the .gitignore!
# When updating subprojects, change the commit being checked out, and set the `shallow-since`
# to the day before, to reduce the amount of refs being transferred and thus speed up CI.

test_downstream() { # owner repo make-target build-file build-hash
	if ! pushd "$2"; then
		echo >&2 'Please run `'"$FETCH_TEST_DEPS"'` before running the test suite'
		return 1
	fi
	make clean $RGBDS_PATH
	make -j4 "$3" $RGBDS_PATH
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
test_downstream AntonioND ucity   all      ucity.gbc 5f026649611c9606ce0bf70dc1552e054e7df5bc
test_downstream pinobatch libbet  all      libbet.gb f117089aa056600e2d404bbcbac96b016fc64611
test_downstream LIJI32    SameBoy bootroms build/bin/BootROMs/cgb_boot.bin 113903775a9d34b798c2f8076672da6626815a91
# gb-starter kit fails with any `make` on Windows: https://codeberg.org/ISSOtm/gb-starter-kit/issues/1
# gb-starter-kit fails with macOS/BSD `make`: https://codeberg.org/ISSOtm/gb-starter-kit/issues/29
if [[ "${osname%-*}" != "windows" && "${osname%-*}" != "macos" && "${osname%-*}" != "bsd" ]]; then
	test_downstream ISSOtm gb-starter-kit all bin/boilerplate.gb b4f130169ba73284e0d0e71b53e7baa4eca2f7fe
fi
