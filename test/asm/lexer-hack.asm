MACRO mac
	println "got {d:_NARG} args: \#"
ENDM

; indented, these were always macro invocations
	mac
	mac ro
	mac : ld a, 1

; in column 1, we historically treated these as labels
mac
mac ro
mac : ld b, 2

SECTION "test", ROM0

; a colon makes these into labels
	Label1: ld c, 3
Label2: ld d, 4

; a macro invocation when already defined as a label
	Label1 args
; and a label definition when already defined as a macro
mac: ld e, 5

; the space before the colon matters! the space before the macro does not
	undef :
undef ::
