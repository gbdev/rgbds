#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C

# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

cd "$(dirname "$0")"

usage() {
	cat <<"EOF"
Runs regression tests on RGBDS.
Options:
    -h, --help            show this help message
    --only-internal       only run tests that build local examples
    --only-external       only run tests that build external codebases
    --only-free           skip tests that build nonfree codebases
    --os <os>             skip tests known to fail on <os> (e.g. `macos-14`)
    --installed-rgbds     use the system installed RGBDS
                          (only compatible with external codebases)
EOF
}

# Parse options in pure Bash because macOS `getopt` is stuck
# in what util-linux `getopt` calls `GETOPT_COMPATIBLE` mode
nonfree=true
internal=true
external=true
installedrgbds=false
osname=
while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		--only-internal)
			external=false
			;;
		--only-external)
			internal=false
			;;
		--only-free)
			nonfree=false
			;;
		--installed-rgbds)
			installedrgbds=true
			;;
		--os)
			shift
			osname="$1"
			;;
		--)
			break
			;;
		*)
			echo "$(basename "$0"): unknown option '$1'"
			exit 1
			;;
	esac
	shift
done

if ! "$internal" && ! "$external"; then
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

# Test some significant external projects that use RGBDS.
# Note that they are intentionally sequenced after the internal tests, since
# failures reported by internal tests will be easier to debug than external ones.
cd external
# Each iteration is isolated in a (subshell) so the sourced cfg variables don't "leak" out.
for cfg in *.cfg; do (
	# Sourcing "$cfg" defines the `EXT_TEST_IS_NONFREE` variable used below.
	. "$cfg"
	test_name="${cfg%.cfg}"

	if [ "$test_name" = gb-starter-kit ]; then
		# gb-starter kit fails with any `make` on Windows: https://codeberg.org/ISSOtm/gb-starter-kit/issues/1
		# gb-starter-kit fails with macOS/BSD `make`: https://codeberg.org/ISSOtm/gb-starter-kit/issues/29
		case "${osname%%-*}" in
			windows | macos | *bsd) exit 0 ;; # Note that this only exits the subshell!
		esac
	fi

	# Run nonfree tests only if they are opted into.
	if ! "$EXT_TEST_IS_NONFREE" || "$nonfree"; then
		./test.sh "$test_name"
	fi
); done
