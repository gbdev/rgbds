SECTION "test", ROMX
	; Foo is unknown so none of these should warn
	assert warn, Foo & $8000
	assert warn, !Foo
	assert warn, LOW(Foo)
	assert warn, !(Foo & LOW(Foo))
