charmap "a", 1
charmap "b", 2, 3
charmap "cdef", 4
charmap "ghi", 5, 6, 7, 8, 9
charmap "jkl", 123, 456, 789
charmap "mno", 123456789

assert charval("a") == 1
assert charval("cdef") == 4

assert charval("a", 0) == 1
assert charval("a", -1) == 1
assert charval("b", 0) == 2
assert charval("b", 1) == 3
assert charval("b", -1) == 3
assert charval("b", -2) == 2
assert charval("cdef", 0) == 4
assert charval("ghi", 2) == charval("ghi", -3)
assert charval("jkl", -1) == 789
assert charval("mno", 0) == 123456789

; errors
assert charval("b") == 0
assert charval("ab") == 0
assert charval("abc", 0) == 0
assert charval("cd", 1) == 0
assert charval("xyz", 2) == 0
assert charval("ghi", -10) == 5
assert charval("ghi", 10) == 0
