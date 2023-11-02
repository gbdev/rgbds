; default charmap 'main'
charmap "a", 1
charmap "ab", 2
charmap "abc", 3

newcharmap second
charmap "d", 4
charmap "e", 5
charmap {__ISO_8601_UTC__}, 6 ; expands with quotes

setcharmap main

assert incharmap("a")
assert incharmap("ab")
assert incharmap(strcat("ab", "c"))

assert !incharmap("") ; empty
assert !incharmap("A") ; case sensitive
assert !incharmap("aa") ; multiple chars "a" "a"
assert !incharmap("d") ; unmapped char "d"
assert !incharmap("bc") ; unmapped chars "b" "c"

setcharmap second

assert incharmap("d") ; now "d" is mapped
assert !incharmap("a") ; only in 'main'
assert !incharmap("bc") ; still unmapped chars
assert incharmap({__ISO_8601_UTC__})
