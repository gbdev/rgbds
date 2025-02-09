charmap "a", 1
charmap "b", 2
charmap "c", 0
charmap "w", 3, 2, 1
charmap "x", 1, 2
charmap "y", 2, 1
charmap "z", 1, 2, 3

macro test
	println strfmt("\"%#s\" <=> \"%#s\" == %d", \1, \2, charcmp(\1, \2))
endm

test "", ""
test "a", "a"
test "aa", "aaa"
test "aaa", "aa"
test "a", "b"
test "b", "a"
test "", "b"
test "c", ""
test "abc", "cba"
test "cabc", "cxc"
test "zy", "abw"
test "abab", "xx"
test "abab", "ww"
test "w", "z"
test "xcy", "zw"
