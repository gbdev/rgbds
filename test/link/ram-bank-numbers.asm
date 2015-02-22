; this should generate a rom consisting of the following bytes:
; 01 02 03 04 05 06 07 00 01 02 03 00 01

section "x",rom0
	db bank(w1),bank(w2),bank(w3),bank(w4),bank(w5),bank(w6),bank(w7)
	db bank(s0),bank(s1),bank(s2),bank(s3)
	db bank(v0),bank(v1)

section "wa",wramx,bank[1]
w1:
section "wb",wramx,bank[2]
w2:
section "wc",wramx,bank[3]
w3:
section "wd",wramx,bank[4]
w4:
section "we",wramx,bank[5]
w5:
section "wf",wramx,bank[6]
w6:
section "wg",wramx,bank[7]
w7:

section "sa",sram,bank[0]
s0:
section "sb",sram,bank[1]
s1:
section "sc",sram,bank[2]
s2:
section "sd",sram,bank[3]
s3:

section "v00",vram,bank[0]
v0:
section "v01",vram,bank[1]
v1:
