IF !DEF(SECOND)
	def ATTRS equs ",ALIGN[2]"
ELSE
	def ATTRS equs "[$CAFE]"
ENDC

SECTION UNION "conflicting alignment", WRAM0 {ATTRS}
	db

	PURGE ATTRS
