; default charmap 'main'
charmap "a", 1
charmap "ab", 2
charmap "abc", 3

newcharmap second
charmap "d", 4
charmap "e", 5
charmap {__ISO_8601_UTC__}, 6 ; expands with quotes

setcharmap main

static_assert incharmap("a")
static_assert incharmap("ab")
static_assert incharmap(strcat("ab", "c"))

static_assert !incharmap("") ; empty
static_assert !incharmap("A") ; case sensitive
static_assert !incharmap("aa") ; multiple chars "a" "a"
static_assert !incharmap("d") ; unmapped char "d"
static_assert !incharmap("bc") ; unmapped chars "b" "c"

setcharmap second

static_assert incharmap("d") ; now "d" is mapped
static_assert !incharmap("a") ; only in 'main'
static_assert !incharmap("bc") ; still unmapped chars
static_assert incharmap({__ISO_8601_UTC__})
