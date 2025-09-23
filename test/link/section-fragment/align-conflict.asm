IF !DEF(SECOND)
	def ATTRS equs ",ALIGN[2]"
ELSE
	def ATTRS equs "[$1337]"
ENDC

SECTION FRAGMENT "conflicting alignment", ROM0 {ATTRS}
	db

	PURGE ATTRS
