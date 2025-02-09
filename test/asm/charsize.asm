charmap "a", 1
charmap "b", 2, 3
charmap "cdef", 4
charmap "ghi", 5, 6, 7, 8, 9
charmap "jkl", 123, 456, 789
charmap "mno", 123456789
charmap "¡Pokémon!", 2, 3

assert charsize("a") == 1
assert charsize("b") == 2
assert charsize("cdef") == 1
assert charsize("ghi") == 5
assert charsize("jkl") == 3
assert charsize("mno") == 1
assert charsize("¡Pokémon!") == 2

assert charsize("") == 0
assert charsize("hello world") == 0
assert charsize("abcdef") == 0
assert charsize("é") == 0
