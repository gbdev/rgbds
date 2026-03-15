#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run a single RGBFIX test case in an isolated temp directory.
#
# Usage:
#   run-one-test.sh <test-type> <test-name> [extra-args...]
#
# test-type:
#   normal   — a .flags file test (runs 3 variants: direct, piped, output)
#   special  — one of the special-case tests (no-exist, no-input, multiple-to-one)
#   padding  — a padding test iteration (test-name = padding index, extra = suffix)
#
# TEST_SRCDIR must point to the test/fix source directory.

set -euo pipefail

# shellcheck source=../helpers.sh
source "$(dirname "$0")/../helpers.sh"

TEST_SRCDIR="${TEST_SRCDIR:?TEST_SRCDIR must be set}"
setup_tools
setup_tmpdir

test_type="${1:?missing test-type argument}"
test_name="${2:?missing test-name argument}"
shift 2

rc=0

# Copy rgbfix and gbdiff into the tmpdir (matching original test.sh pattern)
cp "$RGBFIX" "$TMPWORKDIR/rgbfix"
cp "$GBDIFF" "$TMPWORKDIR/gbdiff.bash"

cd "$TMPWORKDIR"
RGBFIX=./rgbfix
src="$TEST_SRCDIR"

# Set up src path substitution for error message normalization
if command -v cygpath &>/dev/null; then
	subst1="$(printf '%s\n' "$src" | sed 's:[][\/.^$*]:\\&:g')"
	subst2="$(printf '%s\n' "$(cygpath -w "$src")" | sed -e 's:\\:/:g' -e 's:[][\/.^$*]:\\&:g')"
	src_subst="$src/\\|$subst1/\\|$subst2/"
else
	src_subst="$src/"
fi

case "$test_type" in

# =========================================================================
# Normal .flags tests — 3 variants (direct, piped, output)
# =========================================================================
normal)
	test_base="$test_name"

	if grep -qF ' ./' "$src/$test_base.flags"; then
		flags=$(
			head -n 1 "$src/$test_base.flags" |
			sed "s# ./# ${src//#/\\#}/#g"
		)
	else
		flags="@$src/$test_base.flags"
	fi

	for variant in '' ' piped' ' output'; do
		our_rc=0
		echo "${bold}${green}${test_base}${variant}...${rescolors}${resbold}"

		if [[ -r "$src/$test_base.bin" ]]; then
			desired_input="$src/$test_base.bin"
		else
			desired_input="$src/default-input.bin"
		fi

		if [[ -z "$variant" ]]; then
			cp "$desired_input" out.gb
			eval "$RGBFIX" $flags out.gb '>out.out' '2>out.err' || true
			subst=out.gb
		elif [[ "$variant" = ' piped' ]]; then
			# shellcheck disable=SC2002
			cat "$desired_input" | eval "$RGBFIX" $flags - '>out.gb' '2>out.err' || true
			subst='<stdin>'
		elif [[ "$variant" = ' output' ]]; then
			cp "$desired_input" input.gb
			eval "$RGBFIX" $flags -o out.gb input.gb '>out.out' '2>out.err' || true
			subst=input.gb
		fi

		if [[ -r "$src/$test_base.out" ]]; then
			desired_outname="$src/$test_base.out"
		else
			desired_outname=/dev/null
		fi
		if [[ -r "$src/$test_base.err" ]]; then
			desired_errname="$src/$test_base.err"
		else
			desired_errname=/dev/null
		fi

		sed -e "s/$subst/<filename>/g" -e "s#$src_subst##g" out.out |
			tryDiff "$desired_outname" - "$test_base.out${variant}" || our_rc=1
		sed -e "s/$subst/<filename>/g" -e "s#$src_subst##g" out.err |
			tryDiff "$desired_errname" - "$test_base.err${variant}" || our_rc=1

		if [[ -r "$src/$test_base.gb" ]]; then
			tryCmp "$src/$test_base.gb" out.gb "$test_base.gb${variant}" || our_rc=1
		fi

		if [[ $our_rc -ne 0 ]]; then
			rc=1
			break
		fi
	done
	;;

# =========================================================================
# Special tests
# =========================================================================
special)
	echo "${bold}${green}${test_name}...${rescolors}${resbold}"
	case "$test_name" in
	no-exist)
		eval "$RGBFIX" no-exist '2>out.err' || true
		tryDiff "$src/no-exist.err" out.err "${test_name}.err" || rc=1
		;;
	no-input)
		eval "$RGBFIX" '2>out.err' || true
		tryDiff "$src/no-input.err" out.err "${test_name}.err" || rc=1
		;;
	multiple-to-one)
		eval "$RGBFIX" one two three -o multiple-to-one '2>out.err' || true
		tryDiff "$src/multiple-to-one.err" out.err "${test_name}.err" || rc=1
		;;
	*)
		echo "Unknown special test: $test_name" >&2
		exit 2
		;;
	esac
	;;

# =========================================================================
# Padding tests — test_name is the index, $1 is the random padding byte
# =========================================================================
padding)
	padding="${1:?missing padding byte argument}"

	echo "${bold}Checking padding byte $padding...${resbold}"

	cp "$src"/padding{,-large,-larger}.bin .
	touch padding{,-large,-larger}.err

	for suffix in '' -large -larger; do
		cat <<<"  -p $padding" >padding$suffix.flags
		tr '\377' \\$((padding / 64))$(((padding / 8) % 8))$((padding % 8)) <"$src/padding$suffix.gb" >padding$suffix.gb

		# Run all 3 variants for this padding test
		test_base="padding${suffix}"
		flags="@./padding$suffix.flags"

		for variant in '' ' piped' ' output'; do
			our_rc=0

			desired_input="./padding${suffix}.bin"

			if [[ -z "$variant" ]]; then
				cp "$desired_input" out.gb
				eval "$RGBFIX" $flags out.gb '>out.out' '2>out.err' || true
				subst=out.gb
			elif [[ "$variant" = ' piped' ]]; then
				# shellcheck disable=SC2002
				cat "$desired_input" | eval "$RGBFIX" $flags - '>out.gb' '2>out.err' || true
				subst='<stdin>'
			elif [[ "$variant" = ' output' ]]; then
				cp "$desired_input" input.gb
				eval "$RGBFIX" $flags -o out.gb input.gb '>out.out' '2>out.err' || true
				subst=input.gb
			fi

			desired_outname=/dev/null
			desired_errname="./padding${suffix}.err"

			sed -e "s/$subst/<filename>/g" out.out |
				tryDiff "$desired_outname" - "padding${suffix}.out${variant}" || our_rc=1
			sed -e "s/$subst/<filename>/g" out.err |
				tryDiff "$desired_errname" - "padding${suffix}.err${variant}" || our_rc=1

			tryCmp "./padding${suffix}.gb" out.gb "padding${suffix}.gb${variant}" || our_rc=1

			if [[ $our_rc -ne 0 ]]; then
				rc=1
				break
			fi
		done

		if [[ $rc -ne 0 ]]; then
			break
		fi
	done
	;;

*)
	echo "Unknown test type: $test_type" >&2
	exit 2
	;;
esac

exit $rc
