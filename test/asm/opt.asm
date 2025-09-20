SECTION "test", ROM0

pusho
	opt p66, -Q.4, Wno-div
	opt -p  0x42, Q  .0x04, -W  no-div ; idempotent
	ds 1
	println $8000_0000 / -1
	def n = 3.14
	println "{x:n} = {f:n}"
popo

	ds 1
	println $8000_0000 / -1
	def n = 3.14
	println "{x:n} = {f:n}"

pusho -p153
	ds 1
popo
