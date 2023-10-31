IF !DEF(SECOND)
	def DATA = 1
ELSE
	def DATA = 2
ENDC

SECTION UNION "different data", ROM0
	db DATA
