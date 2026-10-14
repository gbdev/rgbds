SECTION "rom", ROMX, BANK[1]
xLabel:
assert @ == xLabel
assert BANK(@) == BANK(xLabel)
assert SECTION(@) === SECTION(xLabel)
	ds $3000, $42

LOAD "ram", WRAMX, BANK[2]
wLabel::
assert @ == wLabel
assert BANK(@) == BANK(wLabel)
assert SECTION(@) === SECTION(wLabel)
	ds $1000, $42

ENDL

SECTION "floating rom", ROMX
xLabel2:
assert @ == xLabel2
assert BANK(@) == BANK(xLabel2)
assert SECTION(@) === SECTION(xLabel2)
	ds $3000, $42

LOAD "floating ram", WRAMX
wLabel2::
assert @ == wLabel2
assert BANK(@) == BANK(wLabel2)
assert SECTION(@) === SECTION(wLabel2)
	ds $1000, $42

ENDL
