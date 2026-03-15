#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run a single external/downstream project test.
#
# Usage:
#   run-one-external.sh <project-name>
#
# Environment:
#   TEST_SRCDIR  — must point to the test/ source directory
#   RGBDS_PATH   — override for RGBDS= make variable (default: path to built binaries)
#   OS_NAME      — for platform-specific skips

set -euo pipefail

# shellcheck source=helpers.sh
source "$(dirname "$0")/helpers.sh"

TEST_SRCDIR="${TEST_SRCDIR:?TEST_SRCDIR must be set}"
# Do NOT call setup_tools — external projects use RGBDS= make variable,
# not individual RGBASM/RGBLINK/… env vars.  Unset them so that downstream
# Makefiles (which use ?=) are free to construct paths from the RGBDS prefix.
unset RGBASM RGBLINK RGBFIX RGBGFX

project="${1:?missing project name argument}"

cd "$TEST_SRCDIR"

# Default RGBDS_PATH to point at the built binaries two levels up
RGBDS_PATH="${RGBDS_PATH:-RGBDS=../../}"
OS_NAME="${OS_NAME:-}"

test_downstream() { # make-target build-file build-hash
	local target="$1" build_file="$2" expected_hash="$3"

	if ! pushd "$project"; then
		echo >&2 "Please run fetch-test-deps.sh before running external tests"
		return 1
	fi
	make clean $RGBDS_PATH
	make -j4 "$target" $RGBDS_PATH
	hash="$(sha1sum -b "$build_file" | head -c 40)"
	if [ "$hash" != "$expected_hash" ]; then
		echo >&2 "SHA-1 hash of $build_file did not match: $hash"
		popd
		return 1
	fi
	popd
}

case "$project" in
	pokecrystal)
		test_downstream compare pokecrystal.gbc f4cd194bdee0d04ca4eac29e09b8e4e9d818c133
		;;
	pokered)
		test_downstream compare pokered.gbc ea9bcae617fdf159b045185467ae58b2e4a48b9a
		;;
	LADX-Disassembly)
		test_downstream default azle.gbc d90ac17e9bf17b6c61624ad9f05447bdb5efc01a
		;;
	ucity)
		test_downstream all ucity.gbc 5f026649611c9606ce0bf70dc1552e054e7df5bc
		;;
	libbet)
		test_downstream all libbet.gb f117089aa056600e2d404bbcbac96b016fc64611
		;;
	SameBoy)
		test_downstream bootroms build/bin/BootROMs/cgb_boot.bin 113903775a9d34b798c2f8076672da6626815a91
		;;
	gb-starter-kit)
		# gb-starter-kit fails on Windows and macOS/BSD make
		if [[ "${OS_NAME%-*}" = "windows" || "${OS_NAME%-*}" = "macos" || "${OS_NAME%-*}" = "bsd" ]]; then
			echo "Skipping gb-starter-kit on ${OS_NAME}" >&2
			exit 0
		fi
		test_downstream all bin/boilerplate.gb b4f130169ba73284e0d0e71b53e7baa4eca2f7fe
		;;
	*)
		echo "Unknown downstream project: $project" >&2
		exit 2
		;;
esac
