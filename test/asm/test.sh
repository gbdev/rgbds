#!/bin/sh
fname=$(mktemp)
rc=0

for i in *.asm; do
	../../rgbasm $i >$fname 2>&1
	diff -u $fname ${i%.asm}.out
	rc=$(($? || $rc))
done

exit $rc
