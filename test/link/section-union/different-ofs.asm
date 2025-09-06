IF !DEF(SECOND)
	def ATTRS equs ",ALIGN[3,7]"
ELSE
	def ATTRS equs ",ALIGN[3,6]"
ENDC

SECTION UNION "conflicting alignment", WRAM0 {ATTRS}
	db

	PURGE ATTRS
