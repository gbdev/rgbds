section "test", rom0[0]

charmap "a", $61
charmap "b", $62
charmap "c", $63
charmap "啊", $04, $c3
charmap "de", $6564
charmap "fghi", $66, $67, $6968

db "abc啊" ; db $61, $62, $63, $04, $C3
db "abcde" ; db $61, $62, $63, $64 (truncated)
dw "abc啊" ; dw $61, $62, $63, $04, $C3
dw "abcde" ; dw $61, $62, $63, $6564
dw "abcdefghi" ; dw $61, $62, $63, $6564, $66, $67, $6968

dl 0 ; separator

charmap "A", $01234567
charmap "B", $fedcba98
assert "A" == $01234567
assert "B" == $fedcba98
db "AB" ; db $01234567, $fedcba98 (truncated to $67, $98)
dl "AB" ; dl $01234567, $fedcba98

charmap "C", $01, $23, $45, $67
charmap "D", $fe, $dc, $ba, $98
assert "C" == $01234567
assert "D" == $fedcba98
db "CD" ; db $01, $23, $45, $67, $fe, $dc, $ba, $98
dw "CD" ; dw $01, $23, $45, $67, $fe, $dc, $ba, $98

charmap "E", $01, $2345, $6789ab, $cdef
assert "E" == $0145abef
db "E" ; db $01, $2345, $6789ab, $cdef (truncated to $01, $45, $ab, $ef)
dl "E" ; dl $01, $2345, $6789ab, $cdef
