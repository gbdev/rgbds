#!/usr/bin/env bash

[[ -e ./rgbgfx_test ]] || make -C ../.. test/gfx/rgbgfx_test
[[ -e ./randtilegen ]] || make -C ../.. test/gfx/randtilegen

trap 'rm -f "$errtmp"' EXIT
errtmp="$(mktemp)"

bold="$(tput bold)"
resbold="$(tput sgr0)"
red="$(tput setaf 1)"
green="$(tput setaf 2)"
rescolors="$(tput op)"

rc=0
new_test() {
	cmdline="${*@Q}"
	echo "$bold${green}Testing: $cmdline$rescolors$resbold" >&2
}
test() {
	eval "$cmdline"
}
fail() {
	rc=1
	echo "$bold${red}Test $cmdline failed!${1:+ (RC=$1)}$rescolors$resbold"
}


for f in *.bin; do
	new_test ./rgbgfx_test "$f"
	test || fail $?
done

# Re-run the tests, but this time, pass a random (non-zero) tile offset
# A tile offset should not change anything to how the image is displayed
while [[ "$ofs" -eq 0 ]]; do
	ofs=$((RANDOM % 256))
done
for f in *.bin; do
	new_test ./rgbgfx_test "$f" -b "$ofs"
	test || fail $?
done

# Remove temporaries (also ignored by Git) created by the above tests
rm -f out*.png result.png

for f in *.png; do
	flags="$([[ -e "${f%.png}.flags" ]] && echo "@${f%.png}.flags")"
	new_test ../../rgbgfx $flags "$f"

	if [[ -e "${f%.png}.err" ]]; then
		test 2>"$errtmp"
		diff -u --strip-trailing-cr "${f%.png}.err" "$errtmp" || fail
	else
		test || fail $?
	fi
done

exit $rc
