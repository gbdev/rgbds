
	println 1 == 1 || undef_n == 42

	println 1 == 2 && 0 < STRLEN("{undef_s}")

	println ((1 == 1 || (undef_a || undef_b)) || undef_c)

	println 1 == 2 && 0 < STRLEN(STRFMT("%d%s%v", "{undef_s}"))
