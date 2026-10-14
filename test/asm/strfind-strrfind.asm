
	static_assert STRFIND("foo bar baz", "bar") == STRRFIND("foo bar baz", "bar")
	static_assert STRFIND("foo bar bargain", "bar") == 4
	static_assert STRRFIND("foo bar bargain", "bar") == 8
	static_assert STRFIND("foo bar", "qux") == -1
	static_assert STRRFIND("foo bar", "qux") == -1
	static_assert STRFIND("foo", "foobar") == -1
	static_assert STRRFIND("foo", "foobar") == -1
	static_assert STRFIND("foobar", "") == 0
	static_assert STRRFIND("foobar", "") == STRLEN("foobar")
