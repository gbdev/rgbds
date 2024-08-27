#!/usr/bin/env bash

[[ -e ./rgbgfx_test ]] || make -C ../.. test/gfx/rgbgfx_test Q= ${CXX:+"CXX=$CXX"} || exit
[[ -e ./randtilegen ]] || make -C ../.. test/gfx/randtilegen Q= ${CXX:+"CXX=$CXX"} || exit

errtmp="$(mktemp)"

# Immediate expansion is the desired behavior.
# shellcheck disable=SC2064
trap "rm -f ${errtmp@Q} result.{png,1bpp,2bpp,pal,tilemap,attrmap,palmap} out*.png" EXIT

tests=0
failed=0
rc=0

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

RGBGFX=../../rgbgfx

newTest () {
	cmdline="$*"
	echo "${bold}${green}Testing: ${cmdline}${rescolors}${resbold}" >&2
}

runTest () {
	(( tests++ ))
	eval "$cmdline"
}

failTest () {
	rc=1
	(( failed++ ))
	echo "${bold}${red}Test ${cmdline} failed!${1:+ (RC=$1)}${rescolors}${resbold}"
}

checkOutput () {
	out_rc=0
	for ext in 1bpp 2bpp pal tilemap attrmap palmap; do
		if [[ -e "$1.out.$ext" ]]; then
			cmp "$1.out.$ext" "result.$ext"
			(( out_rc = out_rc || $? ))
		fi
	done
	return $out_rc
}

# Draw a random tile offset and VRAM0 size
# Neither should change anything to how the image is displayed
while [[ "$ofs" -eq 0 ]]; do (( ofs = RANDOM % 256 )); done
while [[ "$size" -eq 0 ]]; do (( size = RANDOM % 256 )); done
for f in seed*.bin; do
	for flags in ""{," -b $ofs"}{," -N $size,256"}; do
		newTest ./rgbgfx_test "$f" $flags
		runTest || failTest $?
	done
done

for f in *.png; do
	# Do not process outputs of other tests as test inputs themselves
	if [[ "$f" = result.png ]]; then
		continue
	fi

	flags="$([[ -e "${f%.png}.flags" ]] && echo "@${f%.png}.flags")"
	for f_ext in o_1bpp o_2bpp p_pal t_tilemap a_attrmap q_palmap; do
		if [[ -e "${f%.png}.out.${f_ext#*_}" ]]; then
			flags="$flags -${f_ext%_*} result.${f_ext#*_}"
		fi
	done

	newTest "$RGBGFX" $flags "$f"
	if [[ -e "${f%.png}.err" ]]; then
		runTest 2>"$errtmp"
		diff -u --strip-trailing-cr "${f%.png}.err" "$errtmp" || failTest
	else
		runTest && checkOutput "${f%.png}" || failTest $?
	fi

	newTest "$RGBGFX" $flags - "<$f"
	if [[ -e "${f%.png}.err" ]]; then
		runTest 2>"$errtmp"
		diff -u --strip-trailing-cr <(sed "s/$f/<stdin>/g" "${f%.png}.err") "$errtmp" || failTest
	else
		runTest && checkOutput "${f%.png}" || failTest $?
	fi
done

for f in *.[12]bpp; do
	# Do not process outputs or sample outputs of other tests as test inputs themselves
	if [[ "$f" = result.[12]bpp ]] || [[ "$f" = *.out.[12]bpp ]]; then
		continue
	fi

	flags="$([[ -e "${f%.[12]bpp}.flags" ]] && echo "@${f%.[12]bpp}.flags")"

	newTest "$RGBGFX $flags -o $f -r 1 result.png && $RGBGFX $flags -o result.2bpp result.png"
	runTest && cmp "$f" result.2bpp || failTest $?
done

if [[ "$failed" -eq 0 ]]; then
	echo "${bold}${green}All ${tests} tests passed!${rescolors}${resbold}"
else
	echo "${bold}${red}${failed} of the tests failed!${rescolors}${resbold}"
fi

exit $rc
