; ASCII
assert STRUPR("camelCase") === "CAMELCASE"
assert STRLWR("camelCase") === "camelcase"
assert STRFMT("%s", "camelCase") === "camelCase"
assert STRFMT("%A", "camelCase") === "CAMELCASE"
assert STRFMT("%a", "camelCase") === "camelcase"

; UTF-8
assert STRUPR("ÇA a été ?") === "ÇA A éTé ?"
assert STRLWR("ÇA a été ?") === "Ça a été ?"
assert STRFMT("%s", "ÇA a été ?") === "ÇA a été ?"
assert STRFMT("%A", "ÇA a été ?") === "ÇA A éTé ?"
assert STRFMT("%a", "ÇA a été ?") === "Ça a été ?"
