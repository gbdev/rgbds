MACRO printargs
	PRINTLN "first = \<1>"
	FOR I, 2, _NARG
		PRINTLN "next = \<{d:I}>"
	ENDR
	PRINTLN "last = \<{d:_NARG}>"
ENDM

	printargs A, B, C, D

MACRO mac
	println \<2> + \<1_2> + \<\1>
	def x = 2
	println \<{d:x}> + \<1_{d:x}> + \<\<\<13>>>
	def y equs "NARG"
	println \<x> + \<1_{d:x}> + \<\<\<_{y}>>>
ENDM

	mac 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 1

	def nonnumeric equs "1"
	def zero equ 0
	def two equ 2

MACRO bad
	println "nonnumeric", \<nonnumeric>
	println "zero", \<zero>
	println "undefined", \<undefined>
	println "two", \<two>
	println "2", \<2>
ENDM

	bad 42

MACRO toolong
	println \<abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz>
ENDM

	toolong 42
