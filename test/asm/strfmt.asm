DEF VAL EQUS STRFMT("Hello %s! I am %d years old today!", "world", $f)
PRINTLN "{VAL}"

DEF N = -42
PRINTLN STRFMT("signed %010d == unsigned %010u", N, N)

DEF N = 112
DEF FMT EQUS "X"
PRINTLN STRFMT("\tdb %#03{s:FMT} %% 26\t; %#03{FMT}", N, N % 26)

PRINTLN STRFMT("%d = %#x = %#b = %#o != %f", 42, 42, 42, 42, 42.0)

DEF TEMPLATE EQUS "\"%s are %s\\n\""
PRINT STRFMT(TEMPLATE, "roses", "red")
PRINT STRFMT(TEMPLATE, "violets", "blue")
PRINT STRFMT(TEMPLATE, "void", 0, "extra")

PRINTLN STRCAT(STRFMT(STRFMT("%%%s.%d%s", "", 9, "f"), 3.14159), \
	STRFMT(" ~ %s", STRFMT("%s%x", "thr", 238)))

DEF N = 1.23456
PRINTLN STRFMT("%.f -> %.3f -> %f", N, N, N)

PRINTLN STRFMT("%d eol %", 1)

PRINTLN STRFMT("invalid %w spec", 42)

PRINTLN STRFMT("one=%d two=%d three=%d", 1)
