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
		else
			cat $i | ../../rgbasm -o $o - > $after 2>&1
		fi

		diff -u ${i%.asm}.out$variant $after
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
done

rm -f $o $gb $before $after
exit $rc
