
SECTION "test", ROM0

	ds 123

FloatingBase:
	assert WARN, FloatingBase & 0, "Worry about me, but not too much."
	assert FAIL, FloatingBase & 0, "Okay, this is getting serious!"
	assert FATAL, FloatingBase & 0, "It all ends now."
	assert FAIL, FloatingBase & 0, "Not even time to roll credits!"
	assert WARN, 0, "Still can finish the film, though!"
