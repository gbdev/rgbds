assert "hello" === "hello"
assert "hello" !== "goodbye"
assert "game" ++ "boy" === "gameboy"
assert "fire flower" === "fire" ++ " " ++ "flower"
assert "a" === "b" == 0
assert 1 == "a" !== "b"
assert 1 + 2 * 3 ** "x" !== "y" == 7

assert !STRICMP("hello", "hello")
assert !STRICMP("hello", "HELLO")
assert !STRICMP("HeLlO", "hElLo")
assert STRICMP("HELLO", "goodBYE") > 0
assert STRICMP("hello", "GOODbye") > 0
assert STRICMP("", "hello") < 0
assert STRICMP("hello", "") > 0
