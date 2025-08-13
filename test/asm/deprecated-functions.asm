opt Wno-unmapped-char
def s equs "Hello world!"

assert strin(#s, "l") == strfind(#s, "l") + 1
assert strrin(#s, "l") == strrfind(#s, "l") + 1

assert !strcmp(strsub(#s, 7), strslice(#s, 6))
assert !strcmp(strsub(#s, 7, 5), strslice(#s, 6, 11))
assert !strcmp(strsub(#s, strlen(#s), 1), strslice(#s, strlen(#s) - 1, strlen(#s)))
assert !strcmp(strsub(#s, 7, 999), strslice(#s, 6, 999))

assert !strcmp(charsub(#s, 12), strchar(#s, 11))
assert !strcmp(charsub(#s, -1), strchar(#s, -1))
assert !strcmp(charsub(#s, -999), strchar(#s, -999))
assert !strcmp(charsub(#s, 999), strchar(#s, 999))
