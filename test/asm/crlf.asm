; This file is encoded with DOS CR+LF line endings!

DEF s EQUS "Hello, \
world!"
static_assert !strcmp("{s}", "Hello, world!")

/*
 * block comment
 */

REPT 2
	REDEF t EQUS """Hello,
world!"""
	static_assert !strcmp("{t}", "Hello,\nworld!")
ENDR

MACRO m
	static_assert "\1" === "Hello, world!"
ENDM

m Hello\, \
world!
