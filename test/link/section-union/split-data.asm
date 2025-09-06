IF !DEF(SECOND)
	def DATA equs "ds 1\ndb $aa"
ELSE
	def DATA equs "db $bb\nds 1"
ENDC

SECTION UNION "mutually-overlaid data", ROM0
	{DATA}

	PURGE DATA
