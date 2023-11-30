macro def_sect
	IF _NARG == 2
		SECTION "\1", \2
	ELSE
		SECTION "\1", \2, BANK[\3]
	ENDC
	Label\@::
	PRINTLN "\1 (\2): ", BANK("\1"), " == ", BANK(Label\@)
endm

 def_sect ROM0_ok1, ROM0
 def_sect ROM0_ok2, ROM0[$2000]
 def_sect ROMX_ok1, ROMX[$4567]
 def_sect ROMX_ok2, ROMX,  42
 def_sect ROMX_bad, ROMX
 def_sect VRAM_ok,  VRAM,  1
 def_sect VRAM_bad, VRAM
 def_sect SRAM_ok,  SRAM,  4
 def_sect SRAM_bad, SRAM
 def_sect WRAM0_ok, WRAM0
 def_sect WRAMX_ok, WRAMX, 7
 def_sect WRAMX_bad,WRAMX
 def_sect OAM_ok,   OAM
 def_sect HRAM_ok,  HRAM

	PRINTLN "def_sect: ", BANK(def_sect) ; not a label
