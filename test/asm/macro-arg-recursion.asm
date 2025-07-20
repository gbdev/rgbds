MACRO m
	def x = (\1) * 2
	println "{d:x}"
ENDM
	m 5       ; prints 10
	m \\2, 6  ; should not prints 12
