SECTION "Fixed bank", ROMX,BANK[42]
X = BANK(@)

SECTION "Something else", OAM
Y = BANK("Fixed bank")

    PRINTT "@: {X}\nStr: {Y}\n"
