section "good", romx, align[8, 1]
Alpha:
static_assert LOW(Alpha) == 1
db 99
Beta:
static_assert LOW(Beta) == 2

section "bad", romx, align[7, 3]
Gamma:
static_assert LOW(Gamma) == 3
