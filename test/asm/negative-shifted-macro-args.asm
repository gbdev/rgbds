MACRO test
	static_assert _NARG == 10
	println _NARG
	println \1, \<10>, \<-1>, \<-10>
	println \<999>
	println \<-999>
	shift 3
	println _NARG
	println \1, \7, \<-1>, \<-7>
	println \<8>
	println \<-8>
	println \<-999>
	shift 7
	println _NARG
	println \1
	println \<-1>
	shift -10
	println _NARG
	println \<-2_147_483_648>
ENDM

test "a", "b", "c", "d", "e", "f", "g", "h", "i", "j"
