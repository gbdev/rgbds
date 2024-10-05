MACRO test
	assert (\1) == (\2)
ENDM

test 0xDEADbeef, $DEADbeef
test 0o755, &755
test 0b101010, %101010

test 0XcafeBABE, $cafeBABE
test 0O644, &644
test 0B11100100, %11100100

pusho b.X
test 0b.X.X, %.X.X
test 0BX.X., %X.X.
popo
