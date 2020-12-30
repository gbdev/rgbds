SECTION "Test", ROM0

list: MACRO
	db _NARG
if _NARG > 0
	db \#
endc
	db -1
ENDM

	list
	list 42
	list $aa, $bb, $cc, $dd, $ee

person: MACRO
	db \1, \2, \3, \4, \5
ENDM

object: MACRO
x = \1
y = \2
	shift 2
	person y, x, \#
ENDM

	person  5, 10, $33, $44, $55
	object 12,  6, $66, $77, $88

echo: MACRO
	println "\#"
ENDM

R EQUS "S"

	echo P
	echo Q,R, {R},  T
	echo 42,$2a

printall: MACRO
	println \#
ENDM

	printall
	printall "A"
	printall "B", "C",  "D"
