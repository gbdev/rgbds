DEF n = -99 >> 1 ; -Wshift
DEF n = 999_999_999_999 ; -Wlarge-constant

MACRO test
	WARN "warning or error?"
ENDM

test

OPT Werror
test

OPT Wno-error
test
