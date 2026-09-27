assert !def(__CYCLES__)

SECTION "test", ROM0
assert def(__CYCLES__)

assert __CYCLES__ == 0
nop
assert __CYCLES__ == 1
ld a, 42
assert __CYCLES__ == 3
dec a
assert __CYCLES__ == 4
db $c9
assert __CYCLES__ == 4 ; data does not increment __CYCLES__

PUSHS
assert !def(__CYCLES__)

SECTION FRAGMENT "fragmented", ROM0
assert def(__CYCLES__)

assert __CYCLES__ == 0
add [hl]
assert __CYCLES__ == 2
add b
assert __CYCLES__ == 3

DEF __CYCLES__ = 1
assert __CYCLES__ == 1
ccf
assert __CYCLES__ == 2

POPS
assert __CYCLES__ == 4

SECTION UNION "united", WRAM0
assert __CYCLES__ == 0
DEF __CYCLES__ = 42
assert __CYCLES__ == 42

SECTION FRAGMENT "fragmented", ROM0
assert __CYCLES__ == 2 ; this fragment continues from the previous one

SECTION UNION "united", WRAM0
assert __CYCLES__ == 42 ; this union continues from the previous one

SECTION "rom code", ROM0
assert __CYCLES__ == 0
ld hl, STARTOF("ram code")
assert __CYCLES__ == 3

LOAD "ram code", HRAM
assert __CYCLES__ == 0
ld hl, @
assert __CYCLES__ == 3
inc hl
assert __CYCLES__ == 5

ENDL
assert __CYCLES__ == 3

ENDSECTION
assert !def(__CYCLES)
