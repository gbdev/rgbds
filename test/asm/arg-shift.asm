print_all: MACRO
	REPT _NARG
		PRINTT " \1"
		SHIFT
	ENDR
	PRINTT "\n"
ENDM

print_some: MACRO
	PRINTT "\1"
	SHIFT 5
	PRINTT "\2\6\9"
	SHIFT 17
	SHIFT
	PRINTT "\3\9"
ENDM

bad: MACRO
	shift _NARG - 1
	PRINTT \1
	PRINTT "\n"
ENDM

	print_all This test, probably, passes\,, but who knows, ?

	print_some R,e,d,n,e,x,G,a,m,e,B,o,y,D,e,v,e,l,o,p,e,m,e,n,t,S,y,s,t,e,m,\n

	bad 1, 3, 5, 1, 2, 4, 5, 6, 3, 3, 3, 6, 2, 1, "H"
	bad "E"
	bad 0, 1, 2, 3, "L"
	bad 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, "L"
	bad as, asd, asdf, asdfg, asdgh, "O"
