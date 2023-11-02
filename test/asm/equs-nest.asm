; the nested EQUS can't use DEF because Y1 would not be expanded
def X1 equs "Y1 equs \"\\\"Success!\\\\n\\\"\""
def Y1 equs "Z1"
X1
	PRINT Z1
