static_assert bytelen("") == 0
static_assert bytelen("ABC") == 3
static_assert strbyte("ABC", 0) == $41
static_assert strbyte("ABC", -1) == $43

charmap "ABC", 42
static_assert bytelen("ABC") == 3

; characters:
; 1: U+72AC kanji (0xE7 0x8A 0xAC)
; 2: U+1F499 emoji (0xF0 0x9F 0x92 0x99)
; 3: U+0021
def utf8 equs "çŠ¬ğŸ’™!"
static_assert bytelen(#utf8) == 8
static_assert strbyte(#utf8, 0) == $e7
static_assert strbyte(#utf8, 4) == $9f
static_assert strbyte(#utf8, -2) == $99
static_assert strbyte(#utf8, -1) == $21

; characters:
; 1: U+0041 A
; 2: U+0020 space
; 3: invalid byte 0xFE
; 4: invalid byte 0x81
; 5: invalid byte 0xFF
; 6: U+0020 space
; 7: U+6F22 kanji (0xE6 0xBC 0xA2)
def invalid EQUS "A şÿ æ¼¢"
static_assert bytelen(#invalid) == 9
static_assert strbyte(#invalid, 0) == $41
static_assert strbyte(#invalid, 4) == $ff
static_assert strbyte(#invalid, 8) == $a2

; out of bounds
static_assert strbyte("abc", -10) == $61 ; -10 clamped to 0
static_assert strbyte("abc", 10) == 0
