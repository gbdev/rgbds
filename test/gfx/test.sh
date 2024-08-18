#!/usr/bin/env bash

[[ -e ./rgbgfx_test ]] || make -C ../.. test/gfx/rgbgfx_test Q= ${CXX:+"CXX=$CXX"} || exit
[[ -e ./randtilegen ]] || make -C ../.. test/gfx/randtilegen Q= ${CXX:+"CXX=$CXX"} || exit

outtmp="$(mktemp)"
errtmp="$(mktemp)"
tests=0
failed=0
rc=0

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "rm -f ${outtmp@Q} ${errtemp@Q}" EXIT

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBGFX=../../rgbgfx

newTest () {
	testName="$*"
	echo "${bold}${green}Testing ${testName}...${rescolors}${resbold}" >&2
}
runTest () {
	(( tests++ ))
	eval "$*"
}
failTest () {
	rc=1
	(( failed++ ))
	echo "${bold}${red}Test ${testName} failed!${1:+ (RC=$1)}${rescolors}${resbold}"
}

# Draw a random tile offset and VRAM0 size
# Neither should change anything to how the image is displayed
while [[ "$ofs" -eq 0 ]]; do (( ofs = RANDOM % 256 )); done
while [[ "$size" -eq 0 ]]; do (( size = RANDOM % 256 )); done
for f in seed*.bin; do
	for flags in ""{," -b $ofs"}{," -N $size,256"}; do
		newTest "$f, randomized"
		runTest ./rgbgfx_test "$f" $flags || failTest $?
	done
done

# Test round-tripping '-r' with '-c #none'
reverse_cmd="$RGBGFX -c#none,#fff,#000 -o none_round_trip.2bpp -r 1 out.png"
reconvert_cmd="$RGBGFX -c#none,#fff,#000 -o result.2bpp out.png"
compare_cmd="cmp none_round_trip.2bpp result.2bpp"
newTest "none_round_trip.2bpp, reversed"
runTest "$reverse_cmd && $reconvert_cmd && $compare_cmd" || failTest $?

# Remove temporaries (also ignored by Git) created by the above tests
rm -f out*.png result.png result.2bpp

for f in *.png; do
	flags="$([[ -e "${f%.png}.flags" ]] && head "${f%.png}.flags")"

	newTest "$f"
	if [[ -e "${f%.png}.err" ]]; then
		runTest "$RGBGFX" $flags "$f" 2>"$errtmp"
		diff -u --strip-trailing-cr "${f%.png}.err" "$errtmp" || failTest
	else
		runTest "$RGBGFX" $flags "$f" || failTest $?
	fi

	newTest "$f, redirected"
	if [[ -e "${f%.png}.err" ]]; then
		runTest "$RGBGFX" $flags - "<$f" 2>"$errtmp"
		diff -u --strip-trailing-cr <(sed "s/$f/<stdin>/g" "${f%.png}.err") "$errtmp" || failTest
	else
		runTest "$RGBGFX" $flags - "<$f" || failTest $?
	fi
done

if [[ "$failed" -eq 0 ]]; then
	echo "${bold}${green}All ${tests} tests passed!${rescolors}${resbold}"
else
	echo "${bold}${red}${failed} of the tests failed!${rescolors}${resbold}"
fi

exit $rc
