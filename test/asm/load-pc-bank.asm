SECTION "rom", ROMX, BANK[1]
xLabel:
assert @ == xLabel
static_assert BANK(@) == BANK(xLabel)
static_assert SECTION(@) === SECTION(xLabel)
	ds $3000, $42

LOAD "ram", WRAMX, BANK[2]
wLabel::
assert @ == wLabel
static_assert BANK(@) == BANK(wLabel)
static_assert SECTION(@) === SECTION(wLabel)
	ds $1000, $42

ENDL

SECTION "floating rom", ROMX
xLabel2:
assert @ == xLabel2
assert BANK(@) == BANK(xLabel2)
static_assert SECTION(@) === SECTION(xLabel2)
	ds $3000, $42

LOAD "floating ram", WRAMX
wLabel2::
assert @ == wLabel2
assert BANK(@) == BANK(wLabel2)
static_assert SECTION(@) === SECTION(wLabel2)
	ds $1000, $42

ENDL
