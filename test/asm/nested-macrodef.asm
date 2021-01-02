outer_ok: MACRO
definition equs "inner_ok: MACRO\nPRINTLN \"Hello!\"\nENDM"
definition
	PURGE definition
ENDM

	outer_ok
	inner_ok


outer_arg: MACRO
definition equs "inner_arg: MACRO\nPRINTLN \"outer: \1\\ninner: \\1\"\nENDM"
definition
	PURGE definition
ENDM

	outer_arg outside
	inner_arg inside


outer: MACRO
	WARN "Nested macros shouldn't work, whose argument would be \\1?"
inner: MACRO
ENDM

	outer
	inner
