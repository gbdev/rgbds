MACRO setup
	def result = (\2) \1 (\3) \1 (\4)
	def leftgroup = ((\2) \1 (\3)) \1 (\4)
	def rightgroup = (\2) \1 ((\3) \1 (\4))
ENDM

MACRO left
	setup \#
	ASSERT result == leftgroup && result != rightgroup
ENDM

MACRO right
	setup \#
	ASSERT result == rightgroup && result != leftgroup
ENDM

	left /, 24, 6, 2
	left %, 22, 13, 5

	right **, 2, 3, 2

	left ==, 0, 1, 2
	left !=, 1, 1, 2
	left <, 1, 2, 2
	left >, 2, 2, 1
	left <=, 1, 3, 2
	left >=, 2, 3, 1

	left <<, 1, 2, 2
	left >>, 16, 2, 2
	left >>>, 16, 2, 2
