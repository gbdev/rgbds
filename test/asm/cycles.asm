static_assert !def(__CYCLES__)

SECTION "test", ROM0
static_assert def(__CYCLES__)

static_assert __CYCLES__ == 0
nop
static_assert __CYCLES__ == 1
ld a, 42
static_assert __CYCLES__ == 3
dec a
static_assert __CYCLES__ == 4
db $c9
static_assert __CYCLES__ == 4 ; data does not increment __CYCLES__

PUSHS
static_assert !def(__CYCLES__)

SECTION FRAGMENT "fragmented", ROM0
static_assert def(__CYCLES__)

static_assert __CYCLES__ == 0
add [hl]
static_assert __CYCLES__ == 2
add b
static_assert __CYCLES__ == 3

DEF __CYCLES__ = 1
static_assert __CYCLES__ == 1
ccf
static_assert __CYCLES__ == 2

POPS
static_assert __CYCLES__ == 4

SECTION UNION "united", WRAM0
static_assert __CYCLES__ == 0
DEF __CYCLES__ = 42
static_assert __CYCLES__ == 42

SECTION FRAGMENT "fragmented", ROM0
static_assert __CYCLES__ == 2 ; this fragment continues from the previous one

SECTION UNION "united", WRAM0
static_assert __CYCLES__ == 42 ; this union continues from the previous one

SECTION "rom code", ROM0
static_assert __CYCLES__ == 0
ld hl, STARTOF("ram code")
static_assert __CYCLES__ == 3

LOAD "ram code", HRAM
static_assert __CYCLES__ == 0
ld hl, @
static_assert __CYCLES__ == 3
inc hl
static_assert __CYCLES__ == 5

ENDL
static_assert __CYCLES__ == 3

ENDSECTION
static_assert !def(__CYCLES)
