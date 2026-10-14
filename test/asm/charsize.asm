charmap "a", 1
charmap "b", 2, 3
charmap "cdef", 4
charmap "ghi", 5, 6, 7, 8, 9
charmap "jkl", 123, 456, 789
charmap "mno", 123456789
charmap "¡Pokémon!", 2, 3

static_assert charsize("a") == 1
static_assert charsize("b") == 2
static_assert charsize("cdef") == 1
static_assert charsize("ghi") == 5
static_assert charsize("jkl") == 3
static_assert charsize("mno") == 1
static_assert charsize("¡Pokémon!") == 2

static_assert charsize("") == 0
static_assert charsize("hello world") == 0
static_assert charsize("abcdef") == 0
static_assert charsize("é") == 0
