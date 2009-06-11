section			:	T_POP_SECTION string ',' sectiontype
					{ out_NewSection($2,$4); }
				|	T_POP_SECTION string ',' sectiontype '[' const ']'
					{
						if( $6>=0 && $6<0x10000 )
							out_NewAbsSection($2,$4,$6,-1);
						else
							yyerror( "Address must be 16-bit" );
					}
				|	T_POP_SECTION string ',' sectiontype ',' T_OP_BANK '[' const ']'
					{
						if( $4==SECT_CODE )
						{
							if( $8>=1 && $8<=255 )
								out_NewAbsSection($2,$4,-1,$8);
							else
								yyerror( "BANK value out of range" );
						}
						else
							yyerror( "BANK only allowed for CODE/DATA" );
					}
				|	T_POP_SECTION string ',' sectiontype '[' const ']' ',' T_OP_BANK '[' const ']'
					{
						if( $4==SECT_CODE )
						{
							if( $6>=0 && $6<0x10000 )
							{
								if( $11>=1 && $11<=255 )
									out_NewAbsSection($2,$4,$6,$11);
								else
									yyerror( "BANK value out of range" );
							}
							else
								yyerror( "Address must be 16-bit" );
						}
						else
							yyerror( "BANK only allowed for CODE/DATA" );
					}
;

sectiontype		:	T_SECT_BSS	{ $$=SECT_BSS; }
				|	T_SECT_VRAM	{ $$=SECT_VRAM; }
				|	T_SECT_CODE	{ $$=SECT_CODE; }
				|	T_SECT_HOME	{ $$=SECT_HOME; }
				|	T_SECT_HRAM	{ $$=SECT_HRAM; }
;


cpu_command		:	z80_adc
				|	z80_add
				|	z80_and
				|	z80_bit
				|	z80_call
				|	z80_ccf
				|	z80_cp
				|	z80_cpl
				|	z80_daa
				|	z80_dec
				|	z80_di
				|	z80_ei
				|	z80_ex
				|	z80_halt
				|	z80_inc
				|	z80_jp
				|	z80_jr
				|	z80_ld
				|	z80_ldd
				|	z80_ldi
				|	z80_ldio
				|	z80_nop
				|	z80_or
				|	z80_pop
				|	z80_push
				|	z80_res
				|	z80_ret
				|	z80_reti
				|	z80_rl
				|	z80_rla
				|	z80_rlc
				|	z80_rlca
				|	z80_rr
				|	z80_rra
				|	z80_rrc
				|	z80_rrca
				|	z80_rst
				|	z80_sbc
				|	z80_scf
				|	z80_set
				|	z80_sla
				|	z80_sra
				|	z80_srl
				|	z80_stop
				|	z80_sub
				|	z80_swap
				|	z80_xor
;

z80_adc			:	T_Z80_ADC op_a_n	{ out_AbsByte(0xCE); out_RelByte(&$2); }
				|	T_Z80_ADC op_a_r	{ out_AbsByte(0x88|$2); }
;

z80_add			:	T_Z80_ADD op_a_n	{ out_AbsByte(0xC6); out_RelByte(&$2); }
				|	T_Z80_ADD op_a_r	{ out_AbsByte(0x80|$2); }
				|	T_Z80_ADD op_hl_ss	{ out_AbsByte(0x09|($2<<4)); }
				|	T_Z80_ADD T_MODE_SP comma const_8bit
					{ out_AbsByte(0xE8); out_RelByte(&$4); }

;

z80_and			:	T_Z80_AND op_a_n	{ out_AbsByte(0xE6); out_RelByte(&$2); }
				|	T_Z80_AND op_a_r	{ out_AbsByte(0xA0|$2); }
;

z80_bit			:	T_Z80_BIT const_3bit comma reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x40|($2<<3)|$4); }
;

z80_call		:	T_Z80_CALL const_16bit
					{ out_AbsByte(0xCD); out_RelWord(&$2); }
				|	T_Z80_CALL ccode comma const_16bit
					{ out_AbsByte(0xC4|($2<<3)); out_RelWord(&$4); }
;

z80_ccf			:	T_Z80_CCF
					{ out_AbsByte(0x3F); }
;

z80_cp			:	T_Z80_CP op_a_n	{ out_AbsByte(0xFE); out_RelByte(&$2); }
				|	T_Z80_CP op_a_r	{ out_AbsByte(0xB8|$2); }
;

z80_cpl			:	T_Z80_CPL { out_AbsByte(0x2F); }
;

z80_daa			:	T_Z80_DAA { out_AbsByte(0x27); }
;

z80_dec			:	T_Z80_DEC reg_r
					{ out_AbsByte(0x05|($2<<3)); }
				|	T_Z80_DEC reg_ss
					{ out_AbsByte(0x0B|($2<<4)); }
;

z80_di			:	T_Z80_DI
					{ out_AbsByte(0xF3); }
;

z80_ei			:	T_Z80_EI
					{ out_AbsByte(0xFB); }
;

z80_ex			:	T_Z80_EX T_MODE_HL comma T_MODE_SP_IND
					{ out_AbsByte(0xE3); }
				|	T_Z80_EX T_MODE_SP_IND comma T_MODE_HL
					{ out_AbsByte(0xE3); }
;

z80_halt		:	T_Z80_HALT
					{ out_AbsByte(0x76); out_AbsByte(0x00); }
;

z80_inc			:	T_Z80_INC reg_r
					{ out_AbsByte(0x04|($2<<3)); }
				|	T_Z80_INC reg_ss
					{ out_AbsByte(0x03|($2<<4)); }
;

z80_jp			:	T_Z80_JP const_16bit
					{ out_AbsByte(0xC3); out_RelWord(&$2); }
				|	T_Z80_JP ccode comma const_16bit
					{ out_AbsByte(0xC2|($2<<3)); out_RelWord(&$4); }
				|	T_Z80_JP T_MODE_HL_IND
					{ out_AbsByte(0xE9); }
;

z80_jr			:	T_Z80_JR const_PCrel
					{ out_AbsByte(0x18); out_PCRelByte(&$2); }
				|	T_Z80_JR ccode comma const_PCrel
					{ out_AbsByte(0x20|($2<<3)); out_PCRelByte(&$4); }
;

z80_ldi			:	T_Z80_LDI T_MODE_HL_IND comma T_MODE_A
					{ out_AbsByte(0x02|(2<<4)); }
				|	T_Z80_LDI T_MODE_A comma T_MODE_HL
					{ out_AbsByte(0x0A|(2<<4)); }
;

z80_ldd			:	T_Z80_LDD T_MODE_HL_IND comma T_MODE_A
					{ out_AbsByte(0x02|(3<<4)); }
				|	T_Z80_LDD T_MODE_A comma T_MODE_HL
					{ out_AbsByte(0x0A|(3<<4)); }
;

z80_ldio		:	T_Z80_LDIO T_MODE_A comma op_mem_ind
					{
						rpn_CheckHRAM(&$4,&$4);

						if( (!rpn_isReloc(&$4))
						&&	($4.nVal<0 || ($4.nVal>0xFF && $4.nVal<0xFF00) || $4.nVal>0xFFFF) )
						{
							yyerror( "Source must be in the IO/HRAM area" );
						}

						out_AbsByte(0xF0);
						$4.nVal&=0xFF;
						out_RelByte(&$4);
					}
				|	T_Z80_LDIO op_mem_ind comma T_MODE_A
					{
						rpn_CheckHRAM(&$2,&$2);

						if( (!rpn_isReloc(&$2))
						&&	($2.nVal<0 || ($2.nVal>0xFF && $2.nVal<0xFF00) || $2.nVal>0xFFFF) )
						{
							yyerror( "Destination must be in the IO/HRAM area" );
						}

						out_AbsByte(0xE0);
						$2.nVal&=0xFF;
						out_RelByte(&$2);
					}
;

z80_ld			:	z80_ld_mem
				|	z80_ld_cind
				|	z80_ld_rr
				|	z80_ld_ss
				|	z80_ld_hl
				|	z80_ld_sp
				|	z80_ld_r
				|	z80_ld_a
;

z80_ld_hl		:	T_Z80_LD T_MODE_HL comma '[' T_MODE_SP const_8bit ']'
					{ out_AbsByte(0xF8); out_RelByte(&$6); }
				|	T_Z80_LD T_MODE_HL comma const_16bit
					{ out_AbsByte(0x01|(REG_HL<<4)); out_RelWord(&$4) }
;
z80_ld_sp		:	T_Z80_LD T_MODE_SP comma T_MODE_HL
					{ out_AbsByte(0xF9); }
				|	T_Z80_LD T_MODE_SP comma const_16bit
					{ out_AbsByte(0x01|(REG_SP<<4)); out_RelWord(&$4) }
;

z80_ld_mem		:	T_Z80_LD op_mem_ind comma T_MODE_SP
					{ out_AbsByte(0x08); out_RelWord(&$2); }
				|	T_Z80_LD op_mem_ind comma T_MODE_A
					{
						if( (!rpn_isReloc(&$2)) && $2.nVal>=0xFF00)
						{
							out_AbsByte(0xE0);
							out_AbsByte($2.nVal&0xFF);
						}
						else
						{
							out_AbsByte(0xEA);
							out_RelWord(&$2);
						}
					}
;

z80_ld_cind		:	T_Z80_LD T_MODE_C_IND comma T_MODE_A
					{ out_AbsByte(0xE2); }
;

z80_ld_rr		:	T_Z80_LD reg_rr comma T_MODE_A
					{ out_AbsByte(0x02|($2<<4)); }
;

z80_ld_r		:	T_Z80_LD reg_r comma const_8bit
					{ out_AbsByte(0x06|($2<<3)); out_RelByte(&$4); }
				|	T_Z80_LD reg_r comma reg_r
					{
						if( ($2==REG_HL_IND) && ($4==REG_HL_IND) )
						{
							yyerror( "LD (HL),(HL) not allowed" );
						}
						else
							out_AbsByte(0x40|($2<<3)|$4);
					}
;

z80_ld_a		:	T_Z80_LD reg_r comma T_MODE_C_IND
					{
						if( $2==REG_A )
							out_AbsByte(0xF2);
						else
						{
							yyerror( "Destination operand must be A" );
						}
					}
				|	T_Z80_LD reg_r comma reg_rr
					{
						if( $2==REG_A )
							out_AbsByte(0x0A|($4<<4));
						else
						{
							yyerror( "Destination operand must be A" );
						}
					}
				|	T_Z80_LD reg_r comma op_mem_ind
					{
						if( $2==REG_A )
						{
							if( (!rpn_isReloc(&$4)) && $4.nVal>=0xFF00 )
							{
								out_AbsByte(0xF0);
								out_AbsByte($4.nVal&0xFF);
							}
							else
							{
								out_AbsByte(0xFA);
								out_RelWord(&$4);
							}
						}
						else
						{
							yyerror( "Destination operand must be A" );
						}
					}
;

z80_ld_ss		:	T_Z80_LD reg_ss comma const_16bit
					{ out_AbsByte(0x01|($2<<4)); out_RelWord(&$4) }
;

z80_nop			:	T_Z80_NOP
					{ out_AbsByte(0x00); }
;

z80_or			:	T_Z80_OR op_a_n
					{ out_AbsByte(0xF6); out_RelByte(&$2); }
				|	T_Z80_OR op_a_r
					{ out_AbsByte(0xB0|$2); }
;

z80_pop			:	T_Z80_POP reg_tt
					{ out_AbsByte(0xC1|($2<<4)); }
;

z80_push		:	T_Z80_PUSH reg_tt
					{ out_AbsByte(0xC5|($2<<4)); }
;

z80_res			:	T_Z80_RES const_3bit comma reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x80|($2<<3)|$4); }
;

z80_ret			:	T_Z80_RET
					{ out_AbsByte(0xC9); }
				|	T_Z80_RET ccode
					{ out_AbsByte(0xC0|($2<<3)); }
;

z80_reti		:	T_Z80_RETI
					{ out_AbsByte(0xD9); }
;

z80_rl			:	T_Z80_RL reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x10|$2); }
;

z80_rla			:	T_Z80_RLA
					{ out_AbsByte(0x17); }
;

z80_rlc			:	T_Z80_RLC reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x00|$2); }
;

z80_rlca 		:	T_Z80_RLCA
					{ out_AbsByte(0x07); }
;

z80_rr			:	T_Z80_RR reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x18|$2); }
;

z80_rra			:	T_Z80_RRA
					{ out_AbsByte(0x1F); }
;

z80_rrc			:	T_Z80_RRC reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x08|$2); }
;

z80_rrca 		:	T_Z80_RRCA
					{ out_AbsByte(0x0F); }
;

z80_rst 		:	T_Z80_RST const_8bit
					{
						if( rpn_isReloc(&$2) )
						{
							yyerror( "Address for RST must be absolute" );
						}
						else if( ($2.nVal&0x38)!=$2.nVal )
						{
							yyerror( "Invalid address for RST" );
						}
						else
							out_AbsByte(0xC7|$2.nVal);
					}
;

z80_sbc			:	T_Z80_SBC op_a_n	{ out_AbsByte(0xDE); out_RelByte(&$2); }
				|	T_Z80_SBC op_a_r	{ out_AbsByte(0x98|$2); }
;

z80_scf			:	T_Z80_SCF
					{ out_AbsByte(0x37); }
;

z80_set			:	T_POP_SET const_3bit comma reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0xC0|($2<<3)|$4); }
;

z80_sla			:	T_Z80_SLA reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x20|$2); }
;

z80_sra			:	T_Z80_SRA reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x28|$2); }
;

z80_srl			:	T_Z80_SRL reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x38|$2); }
;

z80_stop		:	T_Z80_STOP
					{ out_AbsByte(0x10); out_AbsByte(0x00); }
;

z80_sub			:	T_Z80_SUB op_a_n	{ out_AbsByte(0xD6); out_RelByte(&$2); }
				|	T_Z80_SUB op_a_r	{ out_AbsByte(0x90|$2); }
;

z80_swap		:	T_Z80_SWAP reg_r
					{ out_AbsByte(0xCB); out_AbsByte(0x30|$2); }
;

z80_xor			:	T_Z80_XOR op_a_n	{ out_AbsByte(0xEE); out_RelByte(&$2); }
				|	T_Z80_XOR op_a_r	{ out_AbsByte(0xA8|$2); }
;

op_mem_ind		:	'[' const_16bit ']'	{ $$ = $2 }
;

op_hl_ss 		:	reg_ss					{ $$ = $1 }
				|	T_MODE_HL comma reg_ss	{ $$ = $3 }
;

op_a_r			:	reg_r				{ $$ = $1 }
				|	T_MODE_A comma reg_r	{ $$ = $3 }
;

op_a_n			:	const_8bit				{ $$ = $1 }
				|	T_MODE_A comma const_8bit	{ $$ = $3 }
;

comma			:	','
;

ccode			:	T_CC_NZ		{ $$ = CC_NZ }
				|	T_CC_Z		{ $$ = CC_Z }
				|	T_CC_NC		{ $$ = CC_NC }
				|	T_MODE_C	{ $$ = CC_C }
;

reg_r			:	T_MODE_B		{ $$ = REG_B }
				|	T_MODE_C		{ $$ = REG_C }
				|	T_MODE_D		{ $$ = REG_D }
				|	T_MODE_E		{ $$ = REG_E }
				|	T_MODE_H		{ $$ = REG_H }
				|	T_MODE_L		{ $$ = REG_L }
				|	T_MODE_HL_IND	{ $$ = REG_HL_IND }
				|	T_MODE_A		{ $$ = REG_A }
;

reg_tt			:	T_MODE_BC		{ $$ = REG_BC }
				|	T_MODE_DE		{ $$ = REG_DE }
				|	T_MODE_HL		{ $$ = REG_HL }
				|	T_MODE_AF		{ $$ = REG_AF }
;

reg_ss			:	T_MODE_BC		{ $$ = REG_BC }
				|	T_MODE_DE		{ $$ = REG_DE }
				|	T_MODE_HL		{ $$ = REG_HL }
				|	T_MODE_SP		{ $$ = REG_SP }
;

reg_rr			:	T_MODE_BC_IND		{ $$ = REG_BC_IND }
				|	T_MODE_DE_IND		{ $$ = REG_DE_IND }
				|	T_MODE_HL_INDINC	{ $$ = REG_HL_INDINC }
				|	T_MODE_HL_INDDEC	{ $$ = REG_HL_INDDEC }
;

%%