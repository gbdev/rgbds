#!/bin/sh
o=$(mktemp)
gb=$(mktemp)
before=$(mktemp)
after=$(mktemp)
rc=0

for i in *.asm; do
	../../rgbasm -o $o $i > $after 2>&1
	diff -u ${i%.asm}.out $after
	rc=$(($? || $rc))
	bin=${i%.asm}.out.bin
	if [ -f $bin ]; then
		../../rgblink -o $gb $o > $after 2>&1
		head -c $(wc -c < $bin) $gb > $after 2>&1
		hexdump -C $after > $before && mv $before $after
		hexdump -C $bin > $before
		diff -u $before $after
		rc=$(($? || $rc))
	fi
done

exit $rc
