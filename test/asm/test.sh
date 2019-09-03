#!/bin/sh
o=$(mktemp)
gb=$(mktemp)
before=$(mktemp)
after=$(mktemp)
rc=0

for i in *.asm; do
	for variant in '' '.pipe'; do
		if [ -z "$variant" ]; then
			../../rgbasm -o $o $i > $after 2>&1
			desired_output=${i%.asm}.out
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
			cat $i | ../../rgbasm -o $o - > $after 2>&1

			# Escape regex metacharacters
			desired_output=$before
			subst="$(printf '%s\n' "$i" | sed 's:[][\/.^$*]:\\&:g')"
			sed "s/$subst/-/g" ${i%.asm}.out > $desired_output
		fi

		diff -u $desired_output $after
		rc=$(($? || $rc))
		bin=${i%.asm}.out.bin
		if [ -f $bin ]; then
			../../rgblink -o $gb $o > $after 2>&1
			dd if=$gb count=1 bs=$(printf %s $(wc -c < $bin)) > $after 2>/dev/null
			hexdump -C $after > $before && mv $before $after
			hexdump -C $bin > $before
			diff -u $before $after
			rc=$(($? || $rc))
		fi
	done
done

rm -f $o $gb $before $after
exit $rc
