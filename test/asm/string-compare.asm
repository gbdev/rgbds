static_assert "hello" === "hello"
static_assert "hello" !== "goodbye"
static_assert "game" ++ "boy" === "gameboy"
static_assert "fire flower" === "fire" ++ " " ++ "flower"
static_assert "a" === "b" == 0
static_assert 1 == "a" !== "b"
static_assert 1 + 2 * 3 ** "x" !== "y" == 7

static_assert !STRICMP("hello", "hello")
static_assert !STRICMP("hello", "HELLO")
static_assert !STRICMP("HeLlO", "hElLo")
static_assert STRICMP("HELLO", "goodBYE") > 0
static_assert STRICMP("hello", "GOODbye") > 0
static_assert STRICMP("", "hello") < 0
static_assert STRICMP("hello", "") > 0
