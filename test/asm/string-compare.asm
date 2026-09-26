assert "hello" === "hello"
assert "hello" !== "goodbye"
assert "game" ++ "boy" === "gameboy"
assert "fire flower" === "fire" ++ " " ++ "flower"
assert "a" === "b" == 0
assert 1 == "a" !== "b"
assert 1 + 2 * 3 ** "x" !== "y" == 7

assert !STRCMP("Hello", "Hello")
assert !STRCMP("", "")
assert STRCMP("hello", "HELLO") == 1
assert STRCMP("HELLO", "hello") == -1
assert STRCMP("hell", "hello") == -1
assert STRCMP("hello", "hell") == 1
assert STRCMP("hello", "goodbye") == 1

assert !STRICMP("hello", "hello")
assert !STRICMP("hello", "HELLO")
assert !STRICMP("HeLlO", "hElLo")
assert !STRICMP("", "")
assert STRICMP("HELLO", "goodBYE") == 1
assert STRICMP("hello", "GOODbye") == 1
assert STRICMP("", "hello") == -1
assert STRICMP("hello", "") == 1
