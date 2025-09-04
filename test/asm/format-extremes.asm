MACRO test
	def v = \1
	println "{#09x:v} = {#012o:v} = {#033b:v} = {u:v}U = {+d:v} = {+.16f:v}"
ENDM
	test $7fff_ffff ; INT32_MAX
	test $8000_0000 ; INT32_MIN
	test $0000_0000 ; UINT32_MIN
	test $ffff_ffff ; UINT32_MAX

println strfmt("%#.255q1f", $7fff_ffff)
