IF !DEF(SECOND)
	def DATA equs "ds 4"
ELSE
	def DATA equs "db $aa, $bb, $cc, $dd"
ENDC

SECTION UNION "overlaid data", ROM0
	{DATA}

	PURGE DATA
