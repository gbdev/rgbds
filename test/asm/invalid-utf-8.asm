; This test tries to pass invalid UTF-8 through a macro argument
; to exercise the lexer's unknown character reporting
m:MACRO
	\1
ENDM
	m ос
