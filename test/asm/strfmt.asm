VAL EQUS STRFMT("Hello %s! I am %d years old today!", "world", $f)
PRINTT "{VAL}\n"

N = -42
PRINTT STRFMT("signed %010d == unsigned %010u\n", N, N)

N = 112
FMT EQUS "X"
PRINTT STRFMT("\tdb %#03{s:FMT} %% 26\t; %#03{FMT}\n", N, N % 26)

TEMPLATE EQUS "\"%s are %s\\n\""
PRINTT STRFMT(TEMPLATE, "roses", "red")
PRINTT STRFMT(TEMPLATE, "violets", "blue")
PRINTT STRFMT(TEMPLATE, "void", 0, "extra")

PRINTT STRCAT(STRFMT(STRFMT("%%%s.%d%s", "", 9, "f"), _PI), \
	STRFMT(" ~ %s\n", STRFMT("%s%x", "thr", 238)))

PRINTT STRFMT("%d eol %", 1)
PRINTT "\n"

PRINTT STRFMT("invalid %w spec\n", 42)

PRINTT STRFMT("one=%d two=%d three=%d\n", 1)
