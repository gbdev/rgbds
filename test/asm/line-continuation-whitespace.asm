; Test that \ followed by whitespace after a macro invocation at the end of the
; file doesn't cause a segfault.

bar: MACRO
	WARN ""
ENDM

foo: bar baz\
