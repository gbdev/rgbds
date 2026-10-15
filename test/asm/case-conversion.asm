; ASCII
static_assert STRUPR("camelCase") === "CAMELCASE"
static_assert STRLWR("camelCase") === "camelcase"
static_assert STRFMT("%s", "camelCase") === "camelCase"
static_assert STRFMT("%A", "camelCase") === "CAMELCASE"
static_assert STRFMT("%a", "camelCase") === "camelcase"

; UTF-8
static_assert STRUPR("ÇA a été ?") === "ÇA A éTé ?"
static_assert STRLWR("ÇA a été ?") === "Ça a été ?"
static_assert STRFMT("%s", "ÇA a été ?") === "ÇA a été ?"
static_assert STRFMT("%A", "ÇA a été ?") === "ÇA A éTé ?"
static_assert STRFMT("%a", "ÇA a été ?") === "Ça a été ?"
