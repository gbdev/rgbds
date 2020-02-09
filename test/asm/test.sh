#!/bin/sh
export LC_ALL=C

o=$(mktemp)
gb=$(mktemp)
input=$(mktemp)
output=$(mktemp)
errput=$(mktemp)
rc=0

bold=$(tput bold)
resbold=$(tput sgr0)
red=$(tput setaf 1)
rescolors=$(tput op)
tryDiff () {
	diff -u --strip-trailing-cr $1 $2 || (echo -e "${bold}${red}${i%.asm}${variant}.$3 mismatch!${rescolors}${resbold}"; false)
}

tryCmp () {
	cmp $1 $2 || (../../contrib/gbdiff.bash $1 $2; echo -e "${bold}${red}${i%.asm}${variant}.out.bin mismatch!${rescolors}${resbold}"; false)
}

for i in *.asm; do
	for variant in '' '.pipe'; do
		if [ -z "$variant" ]; then
			../../rgbasm -Weverything -o $o $i > $output 2> $errput
			desired_output=${i%.asm}.out
			desired_errput=${i%.asm}.err
		else
			# `include-recursion.asm` refers to its own name inside the test code.
			# Skip testing with stdin input for that file.
			if [ "$i" = "include-recursion.asm" ]; then
				continue
			fi

			# Stop! This is not a Useless Use Of Cat. Using cat instead of
			# stdin redirection makes the input an unseekable pipe - a scenario
			# that's harder to deal with and was broken when the feature was
			# first implemented.
			cat $i | ../../rgbasm -Weverything -o $o - > $output 2> $errput

			# Use two otherwise unused files for temp storage
			desired_output=$input
			desired_errput=$gb
			# Escape regex metacharacters
			subst="$(printf '%s\n' "$i" | sed 's:[][\/.^$*]:\\&:g')"
			# Replace the file name with a dash to match changed output
			sed "s/$subst/-/g" ${i%.asm}.out > $desired_output
			sed "s/$subst/-/g" ${i%.asm}.err > $desired_errput
		fi

		tryDiff $desired_output $output out
		our_rc=$?
		tryDiff $desired_errput $errput err
		our_rc=$(($? || $our_rc))

		bin=${i%.asm}.out.bin
		if [ -f $bin ]; then
			../../rgblink -o $gb $o > $output 2>&1
			dd if=$gb count=1 bs=$(printf %s $(wc -c < $bin)) > $output 2>/dev/null
			tryCmp $bin $output
			our_rc=$(($? || $our_rc))
		fi

		rc=$(($rc || $our_rc))
		if [ $our_rc -ne 0 ]; then break; fi
	done
done

rm -f $o $gb $input $output
exit $rc
