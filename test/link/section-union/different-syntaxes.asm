IF !DEF(SECOND)
	def INSTR equs "sbc a"
ELSE
	def INSTR equs "db $9f"
ENDC

SECTION UNION "different syntaxes", ROM0
	{INSTR}

	PURGE INSTR
