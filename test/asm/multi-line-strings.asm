DEF S EQUS "Hello"

PRINT "\"\"\"\n"

PRINT """{S}
world
"""

PRINT """The multi-line string \ ; line continuations work
can contain:
- "single quotes"
- ""double quotes""
- even escaped \"""triple"\"" ""\"quotes\"\"\"
!"""

PRINT """\n"""

MACRO printarg
	PRINTLN "arg <\1>"
	PRINTLN """arg (\1)"""
ENDM

	printarg "
	printarg """multi-line
string argument"""

DEF EMPTY1 EQUS ""
DEF EMPTY2 EQUS "\ ; comment
"
DEF EMPTY3 EQUS """"""
DEF EMPTY4 EQUS """\ ; comment
"""
	PRINTLN STRCAT("(", "{EMPTY1}", "{EMPTY2}", "{EMPTY3}", "{EMPTY4}", ")")

	warn "check the line number"
