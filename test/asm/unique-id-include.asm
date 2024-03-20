REPT 2
	println "Within REPT"
	INCLUDE "unique-id-include.inc"
	println "Outside INCLUDE: \@"
ENDR

MACRO m1
	println "Within MACRO"
	INCLUDE "unique-id-include.inc"
	println "Outside INCLUDE: \@"
ENDM
	m1

println

REPT 2
	println "Within REPT: \@"
	INCLUDE "unique-id-include.inc"
	println "Outside INCLUDE: \@"
ENDR

MACRO m2
	println "Within MACRO: \@"
	INCLUDE "unique-id-include.inc"
	println "Outside INCLUDE: \@"
ENDM
	m2
