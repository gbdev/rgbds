#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Run a single RGBASM test case in an isolated temp directory.
#
# Usage:
#   run-one-test.sh <test-type> <test-name>
#
# test-type is one of:
#   simple   — a .asm file in the asm test directory
#   cli      — a .flags file in the asm/cli/ subdirectory
#   dep      — one of the dependency-tracking test directories
#   state    — the state-file test
#
# TEST_SRCDIR must point to the test/asm source directory.

set -euo pipefail

# shellcheck source=../helpers.sh
source "$(dirname "$0")/../helpers.sh"

TEST_SRCDIR="${TEST_SRCDIR:?TEST_SRCDIR must be set}"
setup_tools
setup_tmpdir

test_type="${1:?missing test-type argument}"
test_name="${2:?missing test-name argument}"

rc=0

cd "$TEST_SRCDIR"

case "$test_type" in

# =========================================================================
# Simple .asm tests — runs normal + piped variants
# =========================================================================
simple)
	i="${test_name}.asm"

	# Generate version.asm dynamically if needed
	if [[ "$test_name" = "version" ]]; then
		if ! git -c safe.directory='*' describe --tags --abbrev=0 >"$TMPWORKDIR/version.out" 2>/dev/null; then
			echo "${bold}${orange}Warning: cannot run version test!${rescolors}${resbold}" >&2
			exit 0  # Skip gracefully
		fi
		"$RGBASM" --version >>"$TMPWORKDIR/version.out"
		cat >"$TMPWORKDIR/version.asm" <<'EOF'
IF !DEF(__RGBDS_RC__)
	PRINTLN "v{d:__RGBDS_MAJOR__}.{d:__RGBDS_MINOR__}.{d:__RGBDS_PATCH__}"
ELSE
	PRINTLN "v{d:__RGBDS_MAJOR__}.{d:__RGBDS_MINOR__}.{d:__RGBDS_PATCH__}-rc{d:__RGBDS_RC__}"
ENDC
	PRINTLN "rgbasm {__RGBDS_VERSION__}"
EOF
		# Redirect to use the tmpdir copies
		i="$TMPWORKDIR/version.asm"
	fi

	RGBASMFLAGS="-Weverything -Bcollapse"
	if [ -f "${test_name}.flags" ]; then
		RGBASMFLAGS="$RGBASMFLAGS @${test_name}.flags"
	fi

	o="$TMPWORKDIR/out.o"
	gb="$TMPWORKDIR/out.gb"
	output="$TMPWORKDIR/stdout"
	errput="$TMPWORKDIR/stderr"

	for variant in '' ' piped'; do
		echo "${bold}${green}${test_name}${variant}...${rescolors}${resbold}"

		if [ -e "${test_name}.out" ]; then
			desired_outname="${test_name}.out"
		elif [[ "$test_name" = "version" ]]; then
			desired_outname="$TMPWORKDIR/version.out"
		else
			desired_outname=/dev/null
		fi
		if [ -e "${test_name}.err" ]; then
			desired_errname="${test_name}.err"
		else
			desired_errname=/dev/null
		fi

		if [ -z "$variant" ]; then
			"$RGBASM" $RGBASMFLAGS -o "$o" "$i" >"$output" 2>"$errput" || true
			desired_output="$desired_outname"
			desired_errput="$desired_errname"
		else
			# Skip piped variant for certain tests
			if [[ "$test_name" = include-recursion || "$test_name" = make-deps || "$test_name" = notexist ]]; then
				continue
			fi

			# Stop! This is not a Useless Use Of Cat. Using cat instead of
			# stdin redirection makes the input an unseekable pipe.
			# shellcheck disable=SC2002
			cat "$i" | "$RGBASM" $RGBASMFLAGS -o "$o" - >"$output" 2>"$errput" || true

			desired_output="$TMPWORKDIR/desired_out"
			desired_errput="$TMPWORKDIR/desired_err"
			# Escape regex metacharacters
			subst="$(printf '%s\n' "$i" | sed 's:[][\/.^$*]:\\&:g')"
			# Replace the file name with "<stdin>" to match changed output
			sed "s/$subst/<stdin>/g" "$desired_outname" >"$desired_output"
			sed "s/$subst/<stdin>/g" "$desired_errname" >"$desired_errput"
		fi

		our_rc=0
		tryDiff "$desired_output" "$output" "${test_name}${variant}.out" || our_rc=1
		tryDiff "$desired_errput" "$errput" "${test_name}${variant}.err" || (( our_rc = 1 ))

		desired_binname="${test_name}.out.bin"
		if [[ -f "$desired_binname" && $our_rc -eq 0 ]]; then
			if ! "$RGBLINK" -x -o "$gb" "$o"; then
				echo "${bold}${red}\`rgblink -x\` failed!${rescolors}${resbold}" >&2
				our_rc=1
			else
				tryCmp "$desired_binname" "$gb" "${test_name}${variant}.gb" || our_rc=1
			fi
		fi

		if [[ $our_rc -ne 0 ]]; then
			rc=1
			break
		fi
	done
	;;

# =========================================================================
# CLI tests — .flags files in cli/ subdirectory
# =========================================================================
cli)
	i="cli/${test_name}.flags"
	output="$TMPWORKDIR/stdout"
	errput="$TMPWORKDIR/stderr"

	echo "${bold}${green}cli/${test_name}...${rescolors}${resbold}"

	if [ -e "cli/${test_name}.out" ]; then
		desired_output="cli/${test_name}.out"
	else
		desired_output=/dev/null
	fi
	if [ -e "cli/${test_name}.err" ]; then
		desired_errput="cli/${test_name}.err"
	else
		desired_errput=/dev/null
	fi

	"$RGBASM" "@$i" >"$output" 2>"$errput" || true

	our_rc=0
	tryDiff "$desired_output" "$output" "cli/${test_name}.out" || our_rc=1
	tryDiff "$desired_errput" "$errput" "cli/${test_name}.err" || (( our_rc = 1 ))

	rc=$our_rc
	;;

# =========================================================================
# Dependency tracking tests
# =========================================================================
dep)
	o="$TMPWORKDIR/out.o"
	output="$TMPWORKDIR/stdout"
	errput="$TMPWORKDIR/stderr"
	fixed_output="$TMPWORKDIR/fixed_out"

	RGBASMFLAGS="-Weverything -Bcollapse -M -"
	if [ -f "$test_name/a.flags" ]; then
		RGBASMFLAGS="$RGBASMFLAGS @$test_name/a.flags"
	fi

	echo "${bold}${green}${test_name}...${rescolors}${resbold}"
	"$RGBASM" $RGBASMFLAGS -o "$o" "${test_name}"/a.asm >"$output" 2>"$errput" || true

	if command -v cygpath &>/dev/null; then
		subst1="$(printf '%s\n' "$o" | sed 's:[][\/.^$*]:\\&:g')"
		subst2="$(printf '%s\n' "$(cygpath -w "$o")" | sed -e 's:\\:/:g' -e 's:[][\/.^$*]:\\&:g')"
		sed -e "s/$subst1/a.o/g" -e "s/$subst2/a.o/g" "$output" >"$fixed_output"
	else
		subst="$(printf '%s\n' "$o" | sed 's:[][\/.^$*]:\\&:g')"
		sed "s/$subst/a.o/g" "$output" >"$fixed_output"
	fi

	our_rc=0
	tryDiff "${test_name}"/a.out "$fixed_output" "${test_name}.out" || our_rc=1
	tryDiff "${test_name}"/a.err "$errput" "${test_name}.err" || (( our_rc = 1 ))

	rc=$our_rc
	;;

# =========================================================================
# State-file test — runs normal + piped variants
# =========================================================================
state)
	o="$TMPWORKDIR/out.o"
	output="$TMPWORKDIR/stdout"
	errput="$TMPWORKDIR/stderr"

	if command -v cygpath &>/dev/null; then
		state_outname="$(cygpath -w "$o")"
	else
		state_outname="$o"
	fi
	state_features="  all  " # Test trimming whitespace
	RGBASMFLAGS="-Weverything -Bcollapse"

	for variant in '' '.pipe'; do
		echo "${bold}${green}state-file${variant}...${rescolors}${resbold}"
		if [ -z "$variant" ]; then
			"$RGBASM" $RGBASMFLAGS -s "$state_features:$state_outname" state-file/a.asm >"$output" 2>"$errput" || true
		else
			# shellcheck disable=SC2002
			cat state-file/a.asm | "$RGBASM" $RGBASMFLAGS -s "$state_features:$state_outname" - >"$output" 2>"$errput" || true
		fi

		our_rc=0
		tryDiff /dev/null "$output" "state-file${variant}.out" || our_rc=1
		tryDiff /dev/null "$errput" "state-file${variant}.err" || (( our_rc = 1 ))
		tryDiff state-file/a.dump.asm "$o" "state-file${variant}.dump" || (( our_rc = 1 ))

		if [[ $our_rc -ne 0 ]]; then
			rc=1
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
