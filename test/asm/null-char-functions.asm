def s equs "hello\0world"

println #s

MACRO assert_equal
	static_assert !strcmp(\1, \2)
ENDM

static_assert strlen(#s) == 11
static_assert strfind(#s, "o\0w") == 4
static_assert strfind(#s, "orld") == 7
static_assert strrfind(#s, "o\0w") == 4
static_assert strrfind(#s, "o") == 7

assert_equal strcat(#s, "\0lol"), "hello\0world\0lol"
assert_equal #s ++ "\0lol", "hello\0world\0lol"
assert_equal strupr(#s), "HELLO\0WORLD"
assert_equal strlwr("HELLO\0WORLD"), #s
assert_equal strslice(#s, 4, 7), "o\0w"
assert_equal strslice(#s, 6), "world"
assert_equal strrpl(#s, "o", "XX"), "hellXX\0wXXrld"
assert_equal strrpl(#s, "\0", "0"), "hello0world"
assert_equal strfmt("%s", #s), #s
assert_equal strchar(#s, 5), "\0"
assert_equal strchar(#s, -1), "d"
