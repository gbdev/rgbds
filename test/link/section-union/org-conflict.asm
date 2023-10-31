IF !DEF(SECOND)
	def ADDR = $BEEF
ELSE
	def ADDR = $BABE
ENDC

SECTION UNION "conflicting address", SRAM[ADDR]
	db
