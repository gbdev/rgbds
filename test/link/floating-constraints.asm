DEF address EQU $100

SECTION "code", ROM0[address]

Start::
	jp Start

assert STARTOF("code") == address
assert Start == address
