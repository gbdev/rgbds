SECTION "Overflow", ROM0
	ds $6000
LOAD "oops",WRAM0
	ds $2000
	db
	db
ENDL

SECTION "Moar overflow", ROM0
	ds $4000
	ds $4000
LOAD "hmm", WRAM0
	ds $2000
	ds $2000
ENDL
	ds $1000

SECTION "Not overflowing", ROM0
	ds $800
LOAD "lol", WRAM0
	ds $1000
	ds $1000
	ds $1000
ENDL
	ds $800
