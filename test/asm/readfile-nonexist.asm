def s equs readfile("readfile-nonexist.inc")
static_assert strlen(#s) == 0
