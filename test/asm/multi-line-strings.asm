S EQUS "Hello"

PRINTT "\"\"\"\n"

PRINTT """{S}
world
"""

PRINTT """The multi-line string \ ; line continuations work
can contain:
- "single quotes"
- ""double quotes""
- even escaped \"""triple"\"" ""\"quotes\"\"\"
!"""

PRINTT """\n"""

printarg: MACRO
	PRINTT "arg <\1>\n"
	PRINTT """arg (\1)\n"""
ENDM

	printarg "
	printarg """

EMPTY1 EQUS ""
EMPTY2 EQUS "\ ; comment
"
EMPTY3 EQUS """"""
EMPTY4 EQUS """\ ; comment
"""
	PRINTT STRCAT("(", "{EMPTY1}", "{EMPTY2}", "{EMPTY3}", "{EMPTY4}", ")\n")
