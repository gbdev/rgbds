print: MACRO
	PRINTT \1
	PRINTT "\n"
ENDM

	print STRCAT()
	print STRCAT("Durrr")
	print STRCAT("Left"\, "right")
	print STRCAT("Whoa"\, "\, "\, "baby!")
