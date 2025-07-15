assert bytelen("") == 0
assert bytelen("ABC") == 3
assert strbyte("ABC", 0) == $41
assert strbyte("ABC", -1) == $43

charmap "ABC", 42
assert bytelen("ABC") == 3

; characters:
; 1: U+72AC kanji (0xE7 0x8A 0xAC)
; 2: U+1F499 emoji (0xF0 0x9F 0x92 0x99)
; 3: U+0021
def utf8 equs "çŠ¬ğŸ’™!"
assert bytelen(#utf8) == 8
assert strbyte(#utf8, 0) == $e7
assert strbyte(#utf8, 4) == $9f
assert strbyte(#utf8, -2) == $99
assert strbyte(#utf8, -1) == $21

; characters:
; 1: U+0041 A
; 2: U+0020 space
; 3: invalid byte 0xFE
; 4: invalid byte 0x81
; 5: invalid byte 0xFF
; 6: U+0020 space
; 7: U+6F22 kanji (0xE6 0xBC 0xA2)
def invalid EQUS "A şÿ æ¼¢"
assert bytelen(#invalid) == 9
assert strbyte(#invalid, 0) == $41
assert strbyte(#invalid, 4) == $ff
assert strbyte(#invalid, 8) == $a2

; out of bounds
assert strbyte("abc", -10) == $61 ; -10 clamped to 0
assert strbyte("abc", 10) == 0
