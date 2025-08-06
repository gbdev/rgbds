SECTION "test", ROM0

MACRO test
	assert !strcmp(\1, \2)
ENDM

test "a"++"b", "ab"
test "a"++""++"b", "ab"
test "a"++"b", strcat("a", "b")
test "a"++"b"++"c", strcat("a","b","c")
test "" ++ "", ""
test strupr("a") ++ strlwr("B"), "Ab"

def str equs "hi"
test #str ++ strupr(#str), "hiHI"
test "a" ++ """b""" ++ strupr("c") ++ strslice(#str, 0, 0), "abC"

charmap "a", 1
charmap "b", 2
charmap "ab", 12
assert "a" + "b" == 3
assert charval("a" ++ "b") == 12

; errors
assert 2 ++ 2 == 4
ld a, [hl++]
