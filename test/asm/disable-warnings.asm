MACRO test
	assert warn, 0, "-Wassert is on by default"
	warn "-Wuser is on by default"
ENDM

test ; no warnings because of -w
OPT -Weverything
test ; still no warnings because of -w
OPT Werror=everything
test ; now errors can occur
