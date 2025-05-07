#!/usr/bin/env bash

export LC_ALL=C

# Game Boy release date, 1989-04-21T12:34:56Z (for reproducible test results)
export SOURCE_DATE_EPOCH=609165296

o="$(mktemp)"
gb="$(mktemp)"
input="$(mktemp)"
output="$(mktemp)"
errput="$(mktemp)"

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "rm -f ${o@Q} ${gb@Q} ${input@Q} ${output@Q} ${errput@Q}" EXIT

tests=0
failed=0
rc=0

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
orange="$(tput setaf 3)"
rescolors="$(tput op)"

RGBASM=../../rgbasm
RGBLINK=../../rgblink

tryDiff () {
	if ! diff -au --strip-trailing-cr "$1" "$2"; then
		echo "${bold}${red}${i%.asm}${variant}.$3 mismatch!${rescolors}${resbold}"
		false
	fi
}

tryCmp () {
	if ! cmp "$1" "$2"; then
		../../contrib/gbdiff.bash "$1" "$2"
		echo "${bold}${red}${i%.asm}${variant}.$3 mismatch!${rescolors}${resbold}"
		false
	fi
}

# Add the version constants test, outputting the closest tag to the HEAD
if git -c safe.directory='*' describe --tags --abbrev=0 >version.out; then
	$RGBASM --version >>version.out
	cat >version.asm <<EOF
IF !DEF(__RGBDS_RC__)
	PRINTLN "v{d:__RGBDS_MAJOR__}.{d:__RGBDS_MINOR__}.{d:__RGBDS_PATCH__}"
ELSE
	PRINTLN "v{d:__RGBDS_MAJOR__}.{d:__RGBDS_MINOR__}.{d:__RGBDS_PATCH__}-rc{d:__RGBDS_RC__}"
ENDC
	PRINTLN "rgbasm {__RGBDS_VERSION__}"
EOF
else
	echo "${bold}${orange}Warning: cannot run version test!${rescolors}${resbold}"
	rm -f version.asm
fi

for i in *.asm notexist.asm; do
	flags=${i%.asm}.flags
	RGBASMFLAGS=-Weverything
	if [ -f "$flags" ]; then
		RGBASMFLAGS="$(head -n 1 "$flags")" # Allow other lines to serve as comments
	fi
	for variant in '' ' piped'; do
		(( tests++ ))
		echo "${bold}${green}${i%.asm}${variant}...${rescolors}${resbold}"
		if [ -e "${i%.asm}.out" ]; then
			desired_outname=${i%.asm}.out
		else
			desired_outname=/dev/null
		fi
		if [ -e "${i%.asm}.err" ]; then
			desired_errname=${i%.asm}.err
		else
			desired_errname=/dev/null
		fi
		if [ -z "$variant" ]; then
			"$RGBASM" $RGBASMFLAGS -o "$o" "$i" >"$output" 2>"$errput"
			desired_output=$desired_outname
			desired_errput=$desired_errname
		else
			# `include-recursion.asm` refers to its own name inside the test code.
			# "notexist" doesn't exist, so there's no point in trying to `cat` it.
			# Skip testing with stdin input for those file.
			if [[ "$i" = include-recursion.asm || "$i" = notexist.asm ]]; then
				continue
			fi

			# Stop! This is not a Useless Use Of Cat. Using cat instead of
			# stdin redirection makes the input an unseekable pipe - a scenario
			# that's harder to deal with and was broken when the feature was
			# first implemented.
			# shellcheck disable=SC2002
			cat "$i" | "$RGBASM" $RGBASMFLAGS -o "$o" - >"$output" 2>"$errput"

			# Use two otherwise unused files for temp storage
			desired_output="$input"
			desired_errput="$gb"
			# Escape regex metacharacters
			subst="$(printf '%s\n' "$i" | sed 's:[][\/.^$*]:\\&:g')"
			# Replace the file name with "<stdin>" to match changed output
			sed "s/$subst/<stdin>/g" "$desired_outname" >"$desired_output"
			sed "s/$subst/<stdin>/g" "$desired_errname" >"$desired_errput"
		fi

		tryDiff "$desired_output" "$output" out
		our_rc=$?
		tryDiff "$desired_errput" "$errput" err
		(( our_rc = our_rc || $? ))

		desired_binname=${i%.asm}.out.bin
		if [[ -f "$desired_binname" && $our_rc -eq 0 ]]; then
			if ! "$RGBLINK" -o "$gb" "$o"; then
				echo "${bold}${red}\`$RGBLINK -o $gb $o\` failed!${rescolors}${resbold}"
			else
				rom_size=$(printf %s $(wc -c <"$desired_binname"))
				dd if="$gb" count=1 bs="$rom_size" >"$output" 2>/dev/null
				tryCmp "$desired_binname" "$output" gb
				(( our_rc = our_rc || $? ))
			fi
		fi

		(( rc = rc || our_rc ))
		if [[ $our_rc -ne 0 ]]; then
			(( failed++ ))
			break
		fi
	done
done

# These tests do their own thing

i="continues-after-missing-include"
RGBASMFLAGS="-Weverything -M - -MG -MC"
# Piping the .asm file to rgbasm would not make sense for dependency generation,
# so just test the normal variant
(( tests++ ))
echo "${bold}${green}${i%.asm}...${rescolors}${resbold}"
"$RGBASM" $RGBASMFLAGS -o "$o" "$i"/a.asm >"$output" 2>"$errput"
fixed_output="$input"
if which cygpath &>/dev/null; then
	# MinGW needs the Windows path substituted but with forward slash separators;
	# Cygwin has `cygpath` but just needs the original path substituted.
	subst1="$(printf '%s\n' "$o" | sed 's:[][\/.^$*]:\\&:g')"
	subst2="$(printf '%s\n' "$(cygpath -w "$o")" | sed -e 's:\\:/:g' -e 's:[][\/.^$*]:\\&:g')"
	sed -e "s/$subst1/a.o/g" -e "s/$subst2/a.o/g" "$output" >"$fixed_output"
else
	subst="$(printf '%s\n' "$o" | sed 's:[][\/.^$*]:\\&:g')"
	sed "s/$subst/a.o/g" "$output" >"$fixed_output"
fi
tryDiff "$i"/a.out "$fixed_output" out
our_rc=$?
tryDiff "$i"/a.err "$errput" err
(( our_rc = our_rc || $? ))
(( rc = rc || our_rc ))
if [[ $our_rc -ne 0 ]]; then
	(( failed++ ))
fi

i="state-file"
if which cygpath &>/dev/null; then
	# MinGW translates path names before passing them as command-line arguments,
	# but does not do so when they are prefixed, so we have to do it ourselves.
	RGBASMFLAGS="-Weverything -s all:$(cygpath -w "$o")"
else
	RGBASMFLAGS="-Weverything -s all:$o"
fi
for variant in '' '.pipe'; do
	(( tests++ ))
	echo "${bold}${green}${i%.asm}${variant}...${rescolors}${resbold}"
	if [ -z "$variant" ]; then
		"$RGBASM" $RGBASMFLAGS "$i"/a.asm >"$output" 2>"$errput"
	else
		# shellcheck disable=SC2002
		cat "$i"/a.asm | "$RGBASM" $RGBASMFLAGS - >"$output" 2>"$errput"
	fi

	tryDiff /dev/null "$output" out
	our_rc=$?
	tryDiff /dev/null "$errput" err
	(( our_rc = our_rc || $? ))
	tryDiff "$i"/a.dump.asm "$o" err
	(( our_rc = our_rc || $? ))

	(( rc = rc || our_rc ))
	if [[ $our_rc -ne 0 ]]; then
		(( failed++ ))
		break
	fi
done

if [[ "$failed" -eq 0 ]]; then
	echo "${bold}${green}All ${tests} tests passed!${rescolors}${resbold}"
else
	echo "${bold}${red}${failed} of the tests failed!${rescolors}${resbold}"
fi

exit $rc
