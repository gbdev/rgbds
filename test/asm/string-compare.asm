static_assert "hello" === "hello"
static_assert "hello" !== "goodbye"
static_assert "game" ++ "boy" === "gameboy"
static_assert "fire flower" === "fire" ++ " " ++ "flower"
static_assert "a" === "b" == 0
static_assert 1 == "a" !== "b"
static_assert 1 + 2 * 3 ** "x" !== "y" == 7

static_assert !STRCMP("Hello", "Hello")
static_assert !STRCMP("", "")
static_assert STRCMP("hello", "HELLO") == 1
static_assert STRCMP("HELLO", "hello") == -1
static_assert STRCMP("hell", "hello") == -1
static_assert STRCMP("hello", "hell") == 1
static_assert STRCMP("hello", "goodbye") == 1

static_assert !STRICMP("hello", "hello")
static_assert !STRICMP("hello", "HELLO")
static_assert !STRICMP("HeLlO", "hElLo")
static_assert !STRICMP("", "")
static_assert STRICMP("HELLO", "goodBYE") == 1
static_assert STRICMP("hello", "GOODbye") == 1
static_assert STRICMP("", "hello") == -1
static_assert STRICMP("hello", "") == 1
