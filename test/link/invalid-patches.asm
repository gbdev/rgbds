section "zero", rom0
Zero::
db 1 % Zero
db 2 ** (Zero - 1)
db BANK(NonexistentSymbol)
db BANK("NonexistentSection")
db STARTOF("NonexistentSection")
db SIZEOF("NonexistentSection")
ldh [Zero], a
jr Zero + 200
