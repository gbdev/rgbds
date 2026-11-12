; SPDX-License-Identifier: MIT

SECTION FRAGMENT "test", ROM0
	dw target
target:
	ds 2, $bb
