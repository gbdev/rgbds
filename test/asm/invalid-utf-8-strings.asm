; characters:
; 1: U+0061 a
; 2: U+00E4 a with diaresis (0xC3 0xA4)
; 3: U+0062 b
; 4: U+6F22 kanji (0xE6 0xBC 0xA2)
; 5: U+002C ,
; 6: U+0061 a
; 7: invalid byte 0xA3
; 8: invalid byte 0xA4
; 9: U+0062 b
; 10: invalid bytes 0xE6 0xF0
; 11: invalid byte 0xA2
; 12: U+0021 !
DEF invalid EQUS "aäb漢,a��b��!"

DEF n = STRLEN("{invalid}")
DEF copy EQUS STRSUB("{invalid}", 1)

println "\"{#s:invalid}\" == \"{#s:copy}\" ({d:n})"

DEF mid1 EQUS STRSUB("{invalid}", 5, 2)
DEF mid2 EQUS STRSUB("{invalid}", 9, 1)
println "\"{#s:mid2}{#s:mid1}\""

; characters:
; 1: U+0041 A
; 2: U+0020 space
; 3: invalid byte 0xFE
; 4: invalid byte 0x81
; 5: invalid byte 0xFF
; 6: U+0020 space
; 7: U+6F22 kanji (0xE6 0xBC 0xA2)
REDEF invalid EQUS "A ��� 漢"

DEF n = STRLEN("{invalid}")
DEF r = CHARLEN("{invalid}")
println "\"{#s:invalid}\": {d:n} == {d:r}"

REDEF mid1 EQUS CHARSUB("{invalid}", 4)
REDEF mid2 EQUS CHARSUB("{invalid}", 7)
println "\"{#s:mid2}{#s:mid1}\""

; characters:
; 1: U+0061 a
; 2: U+0062 b
; 3: U+0063 c
; 4: incomplete U+6F22 kanji (0xE6 0xBC without 0xA2)
REDEF invalid EQUS "abc�"

DEF n = STRLEN("{invalid}")
DEF r = CHARLEN("{invalid}")
println "\"{#s:invalid}\": {d:n} == {d:r}"

DEF final EQUS STRSUB("{invalid}", 4, 1)
println "\"{#s:invalid}\" ends \"{#s:final}\""
