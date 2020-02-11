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


	print_all This test, probably, passes\,, but who knows, ?

	print_some R,e,d,n,e,x,G,a,m,e,B,o,y,D,e,v,e,l,o,p,e,m,e,n,t,S,y,s,t,e,m,\n
