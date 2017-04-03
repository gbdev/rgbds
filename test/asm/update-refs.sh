fname=$(mktemp)

for i in *.asm; do
	../../rgbasm $i >$fname 2>&1
	mv -f $fname ${i%.asm}.out
done

exit 0
