outer_ok: MACRO
definition equs "inner_ok: MACRO\nPRINTT \"Hello!\\n\"\nENDM"
definition
	PURGE definition
ENDM

	outer_ok
	inner_ok


outer_arg: MACRO
definition equs "inner_arg: MACRO\nPRINTT \"outer: \1\\ninner: \\1\\n\"\nENDM"
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
