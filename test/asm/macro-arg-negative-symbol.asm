MACRO test
	println \<2>, " vs ", \<-2>
	println \<_NARG>, " vs ", \<-_NARG>
ENDM
	test "hello", "goodbye"
