charmap "a", 1
charmap "b", 2
charmap "c", 3
charmap "d", 3
charmap "eeeee", $12345678
charmap "x", 1, 2, 3
charmap "y", 4, 5, 6, 7, 8, $99999999
charmap "zed", $1234, $5678, $9abc, $def0

macro test
	redef expected equs \1
	shift
	assert !strcmp(revchar(\#), "{expected}")
endm

test "a", 1
test "b", 2
test "eeeee", 305419896
test "x", 1, 2, 3
test "y", 4, 5, 6, 7, 8, $99999999
test "zed", 4660, 22136, 39612, 57072
test "", 3 ; multiple
test "", 4 ; none
