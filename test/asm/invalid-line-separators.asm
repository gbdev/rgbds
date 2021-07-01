; These directives expect `T_NEWLINE` specifically, not
; `endofline` which can be `T_NEWLINE | T_EOB | T_PERIOD`,
; since lexing IF/REPT/FOR/MACRO bodies uses real lines.

rept 3 . print "A1" . println "A2" . endr

rept 3 . print "B1" . println "B2"
endr

rept 3
	print "C1" . println "C2" . endr
	println "oops"
endr

for n, 1, \
       10 . println n . endr

if 1 . println "oops again"

macro foo
	println "foo" . endm
endm
	foo

macro bar . println "bar" . endm
	bar

macro baz . println "baz"
endm
	baz
