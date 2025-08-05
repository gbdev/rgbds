
	assert STRFIND("foo bar baz", "bar") == STRRFIND("foo bar baz", "bar")
	assert STRFIND("foo bar bargain", "bar") == 4
	assert STRRFIND("foo bar bargain", "bar") == 8
	assert STRFIND("foo bar", "qux") == -1
	assert STRRFIND("foo bar", "qux") == -1
	assert STRFIND("foo", "foobar") == -1
	assert STRRFIND("foo", "foobar") == -1
	assert STRFIND("foobar", "") == 0
	assert STRRFIND("foobar", "") == STRLEN("foobar")
