static_assert "hello" === "hello"
static_assert "hello" !== "goodbye"
static_assert "game" ++ "boy" === "gameboy"
static_assert "fire flower" === "fire" ++ " " ++ "flower"
static_assert "a" === "b" == 0
static_assert 1 == "a" !== "b"
static_assert 1 + 2 * 3 ** "x" !== "y" == 7
