section "known", romx, align[8, 1]
Alpha:
static_assert LOW(Alpha) == 1
db 99
Beta:
static_assert LOW(Beta) == 2

section "unknown", romx, align[7, 3]
Gamma:
static_assert !ISCONST(LOW(Gamma))
assert LOW(Gamma) == 3
