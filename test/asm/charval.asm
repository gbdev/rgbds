charmap "a", 1
charmap "b", 2, 3
charmap "cdef", 4
charmap "ghi", 5, 6, 7, 8, 9
charmap "jkl", 123, 456, 789
charmap "mno", 123456789

static_assert charval("a") == 1
static_assert charval("cdef") == 4

static_assert charval("a", 0) == 1
static_assert charval("a", -1) == 1
static_assert charval("b", 0) == 2
static_assert charval("b", 1) == 3
static_assert charval("b", -1) == 3
static_assert charval("b", -2) == 2
static_assert charval("cdef", 0) == 4
static_assert charval("ghi", 2) == charval("ghi", -3)
static_assert charval("jkl", -1) == 789
static_assert charval("mno", 0) == 123456789

; errors
static_assert charval("b") == 0
static_assert charval("ab") == 0
static_assert charval("abc", 0) == 0
static_assert charval("cd", 1) == 0
static_assert charval("xyz", 2) == 0
static_assert charval("ghi", -10) == 5
static_assert charval("ghi", 10) == 0
