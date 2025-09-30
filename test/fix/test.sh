#!/usr/bin/env bash

export LC_ALL=C

tmpdir="$(mktemp -d)"

cp ../../{rgbfix,contrib/gbdiff.bash} "$tmpdir"
src="$PWD"
cd "$tmpdir" || exit

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "cd; rm -rf ${tmpdir@Q}" EXIT

if which cygpath &>/dev/null; then
	# MinGW needs the Windows path substituted but with forward slash separators;
	# Cygwin has `cygpath` but just needs the original path substituted.
	subst1="$(printf '%s\n' "$src" | sed 's:[][\/.^$*]:\\&:g')"
	subst2="$(printf '%s\n' "$(cygpath -w "$src")" | sed -e 's:\\:/:g' -e 's:[][\/.^$*]:\\&:g')"
	src_subst="$src/\\|$subst1/\\|$subst2/"
else
	src_subst="$src/"
fi

tests=0
failed=0
rc=0

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBFIX=./rgbfix

tryDiff () {
	if ! diff -au --strip-trailing-cr "$1" "$2"; then
		echo "${bold}${red}${3:-$1} mismatch!${rescolors}${resbold}"
		false
	fi
}

tryCmp () {
	if ! cmp "$1" "$2"; then
		./gbdiff.bash "$1" "$2"
		echo "${bold}${red}${3:-$1} mismatch!${rescolors}${resbold}"
		false
	fi
}

runTest () {
	flags=$(
		head -n 1 "$2/$1.flags" | # Allow other lines to serve as comments
		sed "s# ./# ${src//#/\\#}/#g" # Prepend src directory to path arguments
	)

	for variant in '' ' piped' ' output'; do
		(( tests++ ))
		our_rc=0
		if [[ $progress -ne 0 ]]; then
			echo "${bold}${green}$1${variant}...${rescolors}${resbold}"
		fi
		if [[ -r "$2/$1.bin" ]]; then
			desired_input="$2/$1.bin"
		else
			desired_input="$src/default-input.bin"
		fi
		if [[ -z "$variant" ]]; then
			cp "$desired_input" out.gb
			eval "$RGBFIX" $flags out.gb '>out.out' '2>out.err'
			subst=out.gb
		elif [[ "$variant" = ' piped' ]]; then
			# Stop! This is not a Useless Use Of Cat. Using cat instead of
			# stdin redirection makes the input an unseekable pipe - a scenario
			# that's harder to deal with.
			# shellcheck disable=SC2002
			cat "$desired_input" | eval "$RGBFIX" $flags - '>out.gb' '2>out.err'
			subst='<stdin>'
		elif [[ "$variant" = ' output' ]]; then
			cp "$desired_input" input.gb
			eval "$RGBFIX" $flags -o out.gb input.gb '>out.out' '2>out.err'
			subst=input.gb
		fi

		if [[ -r "$2/$1.out" ]]; then
			desired_outname="$2/$1.out"
		else
			desired_outname=/dev/null
		fi
		if [[ -r "$2/$1.err" ]]; then
			desired_errname="$2/$1.err"
		else
			desired_errname=/dev/null
		fi
		sed -e "s/$subst/<filename>/g" -e "s#$src_subst##g" out.out | tryDiff "$desired_outname" - "$1.out${variant}"
		(( our_rc = our_rc || $? ))
		sed -e "s/$subst/<filename>/g" -e "s#$src_subst##g" out.err | tryDiff "$desired_errname" - "$1.err${variant}"
		(( our_rc = our_rc || $? ))

		if [[ -r "$2/$1.gb" ]]; then
			tryCmp "$2/$1.gb" out.gb "$1.gb${variant}"
			(( our_rc = our_rc || $? ))
		fi

		(( rc = rc || our_rc ))
		if [[ $our_rc -ne 0 ]]; then
			(( failed++ ))
			break
		fi
	done
}

runSpecialTest () {
	name="$1"
	shift
	echo "${bold}${green}${name}...${rescolors}${resbold}"
	eval "$RGBFIX" "$@" '2>out.err'
	rc=$((rc || $? != 1))
	tryDiff "$src/${name}.err" out.err "${name}.err"
	rc=$((rc || $?))
}

rm -f padding*_* # Delete padding test cases generated but not deleted (e.g. interrupted)

progress=1
for i in "$src"/*.flags; do
	runTest "$(basename "$i" .flags)" "$src"
done

# Check that RGBFIX errors out when inputting a non-existent file
runSpecialTest no-exist no-exist

# Check that RGBFIX errors out when not inputting any file
runSpecialTest no-input

# Check that RGBFIX errors out when inputting multiple files with an output file
runSpecialTest multiple-to-one one two three -o multiple-to-one

# Check the result with all different padding bytes
echo "${bold}Checking padding...${resbold}"
cp "$src"/padding{,-large,-larger}.bin .
touch padding{,-large,-larger}.err
progress=0
for (( i=0; i < 10; ++i )); do
	(( padding = RANDOM % 256 ))
	echo "$padding..."
	for suffix in '' -large -larger; do
		cat <<<"-p $padding" >padding$suffix.flags
		tr '\377' \\$((padding / 64))$(((padding / 8) % 8))$((padding % 8)) <"$src/padding$suffix.gb" >padding$suffix.gb # OK because $FF bytes are only used for padding
		runTest padding${suffix} .
	done
done
echo "${bold}Done checking padding!${resbold}"

if [[ "$failed" -eq 0 ]]; then
	echo "${bold}${green}All ${tests} tests passed!${rescolors}${resbold}"
else
	echo "${bold}${red}${failed} of the tests failed!${rescolors}${resbold}"
fi

exit $rc
