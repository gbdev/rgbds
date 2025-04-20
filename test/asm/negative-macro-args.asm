MACRO mac
	for i, -1, -_NARG - 1, -1
		println "{d:i}: \<i> == \<{d:i}>"
	endr
	; error cases
	def i = 0
	println "{d:i}: \<i> == \<{d:i}>"
	def i = -_NARG - 1
	println "{d:i}: \<i> == \<{d:i}>"
	def i = $7fff_ffff
	println "{d:i}: \<i> == \<{d:i}>"
	; signed/unsigned difference error cases
	def i = $8000_0000
	println "{d:i}: \<i> == \<{d:i}>"
	println "{u:i}: \<i> == \<{u:i}>"
	def i = $ffff_ffff
	println "{d:i}: \<i> == \<{d:i}>"
	println "{u:i}: \<i> == \<{u:i}>"
ENDM

mac A, B, C, D, E, F, G
