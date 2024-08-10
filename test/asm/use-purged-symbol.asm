SECTION "test", ROM0

MACRO m
ENDM
PURGE m
	m

DEF argi EQU 1
MACRO m2
	println "(\<argi>)!"
ENDM
	PURGE argi
	m2 hello

DEF n EQU argi

PRINTLN "({argi})"

Label::
PURGE Label
DEF x = Label
DEF x = BANK(Label)

Label2::
PURGE Label2
DEF x EQUS SECTION(Label2) ; fatal
