DEF SLIME_HP  EQU 35
DEF SLIME_ATK EQU 15
DEF SLIME_DEF EQU 26
DEF MIMIC_HP  EQU 20
DEF MIMIC_ATK EQU 21
DEF MIMIC_DEF EQU 28

MACRO with_each
	for i, 1, _NARG
		REDEF temp EQUS STRRPL(\<_NARG>, "?", "\<i>")
		{temp}
	endr
ENDM

MACRO with_each_stat
	with_each HP, ATK, DEF, \1
ENDM

with_each_stat """
	println STRFMT("Average ? is %d", (SLIME_? + MIMIC_?) / 2)
"""

MACRO buff_monster
	REDEF monster EQUS STRUPR(STRSLICE("\1", 0, 1)) ++ STRLWR(STRSLICE("\1", 1))
	; delayed \{interpolation\} is necessary here
	; escaping the '}' is not essential (same as '>' in HTML)
	with_each_stat """
		DEF buffed = \1_? * 120 / 100
		println "{monster}'s ? went from \{d:\1_?\} to \{d:buffed}!"
	"""
ENDM

buff_monster MIMIC
buff_monster SLIME
