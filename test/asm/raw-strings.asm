DEF q EQUS "\""

	assert !strcmp( \
		#"\t\1{s}\", \
		"\\t\\1\{s}\\" )
	assert !strcmp( \
		#"\a,\b,\1,\2", \
		"\\a,\\b,\\1,\\2" )
	assert !strcmp( \
		#"""new
line""", \
		"new\nline" )
	assert !strcmp( \
		#"""new\nline""", \
		"""new\\nline""" )
	assert !strcmp( \
		#"/\w+(\+\w+)?@[a-z]+\.[a-z]{2,3}/i", \
		"/\\w+(\\+\\w+)?@[a-z]+\\.[a-z]\{2,3}/i" )
	assert !strcmp( \
		#{q}{q}{q}rs", \
		{q}\{q}\{q}rs" )
	assert !strcmp( \
		#"", \
		"" )
	assert !strcmp( \
		#"""""", \
		"""""" )

MACRO test
	REDEF raw EQUS \1
	REDEF plain EQUS \2
	assert !strcmp("{raw}", "{plain}")
ENDM

	; test lexing string literals within macro args
	test \
		#"\t\1{s}\", \
		"\\t\\1\{s}\\"
	test \
		#"\a,\b,\1,\2", \
		"\\a,\\b,\\1,\\2"
	test \
		#"""new,
line""", \
		"new,\nline"
	test \
		#"""new,\nline""", \
		"""new,\\nline"""
	test \
		#"/\w+(\+\w+)?@[a-z]+\.[a-z]{2,3}/i", \
		"/\\w+(\\+\\w+)?@[a-z]+\\.[a-z]\{2,3}/i"
	test \
		#{q}{q}{q}rs", \
		{q}\{q}\{q}rs"
	test \
		#"", \
		""
	test \
		#"""""", \
		""""""

MACRO echo
	println "\#"
ENDM

DEF s EQUS "foo"
	echo \
		# "{s}", \
		#"{s}", \ ; raw!
		#raw"{s}", \
		#/*comment*/"{s}"
	echo \
		# """{s}""", \
		#"""{s}""", \ ; raw!
		#raw"""{s}""", \
		#/*comment*/"""{s}"""
