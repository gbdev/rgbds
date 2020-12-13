print: MACRO
	PRINTT \1
	PRINTT "\n"
ENDM

	print STRJOIN("-")
	print STRJOIN("+"\, STRJOIN("*"\,"1"\,"2"\,"3")\, STRJOIN("/"\, STRJOIN("^"\,"4"\,"5")\, "6"))
	print STRJOIN("--"\, "Left"\, "right")
	print STRJOIN(""\, "Whoa"\, "\, "\, "baby!")
