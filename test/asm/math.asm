	assert 2*10**2*2 == 400
	assert -3**4 == -81

	assert DIV(5.0, 2.0) == 2.5
	assert DIV(-5.0, 2.0) == -2.5
	assert DIV(-5.0, 0.0) == $8000_0000

	assert MUL(10.0, 0.5) == 5.0
	assert MUL(10.0, 0.0) == 0.0

	assert POW(10.0, 2.0) == 100.0
	assert POW(100.0, 0.5) == 10.0

	assert LOG(100.0, 10.0) == 2.0
	assert LOG(256.0, 2.0) == 8.0

	assert ROUND(1.5) == 2.0
	assert ROUND(-1.5) == -2.0

	assert CEIL(1.5) == 2.0
	assert CEIL(-1.5) == -1.0

	assert FLOOR(1.5) == 1.0
	assert FLOOR(-1.5) == -2.0
