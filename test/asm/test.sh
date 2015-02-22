fname=$(mktemp)

for i in *.asm; do
	../../rgbasm $i >$fname 2>&1
	diff -u $fname ${i%.asm}.out
done
