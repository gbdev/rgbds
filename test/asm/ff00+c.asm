SECTION "test", ROM0[0]
	ldh [ $ff00 + c ], a
	; 257 spaces exceeds both LEXER_BUF_SIZE (64) and uint8_t limit (255)
	ldh [ $ff00 +                                                                                                                                                                                                                                                                 c ], a
	ldh [ $ff00                                                                                                                                                                                                                                                                 + c ], a
