SECTION "sect", ROMX[$4567], BANK[$23]
	ds 42

W = BANK("sect")
X = SIZEOF("sect")
Y = STARTOF("sect")

	println "{W} {X} {Y}"
