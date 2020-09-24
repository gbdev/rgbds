; Check deleting a macro then using its file stack info
m: MACRO
	PURGE m
	WARN "Where am I?"
ENDM
	m
