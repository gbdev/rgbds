SECTION "Test", ROM0


p EQUS "  2 + 2  "
	ld a, SYM("p")


x = 5
	ld a, x * 2
	ld a, SYM("x") * 2
	bit x, a
	bit SYM(STRLWR("X")), a
	db $aa, x, $ff
	db $aa, SYM(STRSUB(" x ", 2, 1)), $ff


s EQUS "\"HELLO\""
	db $aa, s, $ff
	db $aa, SYM("s"), $ff ; TODO: SYM("s") gives 07 not 48 45 4C 4C 4F


MyLabel:
s_mylabel EQUS "MyLabel"
	db BANK(MyLabel)
	db BANK(s_mylabel)
	db BANK(SYM("s_mylabel"))


METAFOO EQUS "FOO"
METAFOO EQUS "Hello world"
_TMP EQUS "PURGE {METAFOO}"
	_TMP
	PURGE _TMP, METAFOO
	ASSERT !DEF(FOO) && !DEF(METAFOO)

METAFOO EQUS "FOO"
METAFOO EQUS "Hello world"
	PURGE SYM("{METAFOO}"), METAFOO
	ASSERT !DEF(FOO) && !DEF(METAFOO)


TM42_MOVE = $2a
n = 42

MOVE_FOR_TM EQUS "TM{d:n}_MOVE"
	db MOVE_FOR_TM
PURGE MOVE_FOR_TM

	db SYM("TM{d:n}_MOVE")


STRUCT_NAME EQUS "mystructname"
CUR_FIELD_ID = 1
mystructname_field1_name EQUS "myfieldname"
myfieldname EQUS "\"foobar\""

TMP EQUS "{STRUCT_NAME}_field{d:CUR_FIELD_ID}_name"
CONFLICTING_FIELD_NAME EQUS TMP
	PRINTT "CONFLICTING_FIELD_NAME == \"{CONFLICTING_FIELD_NAME}\"\n"
PURGE TMP

CONFLICTING_FIELD_NAME_SYM EQUS SYM(SYM("{STRUCT_NAME}_field{d:CUR_FIELD_ID}_name"))
	PRINTT "CONFLICTING_FIELD_NAME_SYM == \"{CONFLICTING_FIELD_NAME_SYM}\"\n"


_NUM_WARPS EQUS "_NUM_WARPS_1"
_NUM_WARPS = 5

_WARP_TO_NAME EQUS "_WARP_TO_NUM_{d:{_NUM_WARPS}}"
_WARP_TO_NAME EQUS "warp_to 1, 2, _WARP_TO_WIDTH"
	PRINTT "_WARP_TO_NUM_5 == \"{_WARP_TO_NUM_5}\"\n"
PURGE _WARP_TO_NAME

SYM("_WARP_TO_NUM_{d:{_NUM_WARPS}}_SYM") EQUS "warp_to 1, 2, _WARP_TO_WIDTH"
	PRINTT "_WARP_TO_NUM_5_SYM == \"{_WARP_TO_NUM_5}_SYM\"\n"
