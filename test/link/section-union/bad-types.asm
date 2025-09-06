IF !DEF(SECOND)
	def TYPE equs "HRAM"
ELSE
	def TYPE equs "WRAM0"
ENDC

SECTION UNION "conflicting types", {TYPE}
	db

	PURGE TYPE
