IF !DEF(SECOND)
	def SIZE = 69
ELSE
	def SIZE = 420
ENDC

SECTION UNION "different section sizes", ROM0
	ds SIZE
