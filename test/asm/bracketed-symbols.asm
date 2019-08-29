X = 42
PRINTT "{X}\n"
PRINTT "{x:X}\n"
PRINTT "{X:X}\n"
PRINTT "{d:X}\n"
PRINTT "{b:X}\n"

Y equ 1337
PRINTT "{b:Y}\n"

rsreset
R rb 0
PRINTT "{d:R}\n"

S equs "You can't format me!"
PRINTT "{X:S}\n"

SECTION "Test", ROM0
Label:
PRINTT "{x:Label}\n"
