SECTION "Test", ROM0

	assert STRFIND("foo bar baz", "bar") == STRRFIND("foo bar baz", "bar")
	assert STRIN("foo bar baz", "bar") == STRRIN("foo bar baz", "bar")

	assert STRFIND("foo bar bargain", "bar") == 4
	assert STRIN("foo bar bargain", "bar") == 5

	assert STRRFIND("foo bar bargain", "bar") == 8
	assert STRRIN("foo bar bargain", "bar") == 9

	assert STRFIND("foo bar", "qux") == -1
	assert STRIN("foo bar", "qux") == 0

	assert STRRFIND("foo bar", "qux") == -1
	assert STRRIN("foo bar", "qux") == 0

	assert STRFIND("foo", "foobar") == -1
	assert STRIN("foo", "foobar") == 0

	assert STRRFIND("foo", "foobar") == -1
	assert STRRIN("foo", "foobar") == 0

	assert STRFIND("foobar", "") == 0
	assert STRIN("foobar", "") == 1

	assert STRRFIND("foobar", "") == STRLEN("foobar")
	assert STRRIN("foobar", "") == STRLEN("foobar") + 1
