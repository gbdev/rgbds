SECTION "destination optional", ROM0[0]

MACRO both
	REDEF op EQUS "\1"
	SHIFT
	{op} \#
	if _NARG
		{op} a, \#
	else
		{op} a
	endc
ENDM

	both cpl
	both add, b
	both adc, 42
	both sub, 69
	both sbc, c
	both and, d
	both or, %1010
	both xor, $80
