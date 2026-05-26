SECTION "test", ROM0[0]
	ld [ $ff00 + c ], a
	; 257 spaces exceeds the uint8_t limit (255)
	ld [ $ff00 +                                                                                                                                                                                                                                                                 c ], a
	ld [ $ff00                                                                                                                                                                                                                                                                 + c ], a
