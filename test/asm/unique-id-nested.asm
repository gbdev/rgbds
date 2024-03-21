MACRO m1
	PRINTLN "Begin MACRO"
	DEF nested EQUS """MACRO mm
		PRINTLN "Within nested MACRO: \\@"
		\n ENDM
		mm"""
	nested
	PURGE nested, mm
	PRINTLN "Within MACRO: \@"
ENDM
REPT 2
	PRINTLN "Begin REPT"
	m1
	PRINTLN "Within REPT: \@"
ENDR

PRINTLN

MACRO m2
	PRINTLN "Begin MACRO: \@"
	DEF nested EQUS """MACRO mm
		PRINTLN "Within nested MACRO: \\@"
		\n ENDM
		mm"""
	nested
	PURGE nested, mm
	PRINTLN "Within MACRO: \@"
ENDM
REPT 2
	PRINTLN "Begin REPT: \@"
	m2
	PRINTLN "Within REPT: \@"
ENDR
