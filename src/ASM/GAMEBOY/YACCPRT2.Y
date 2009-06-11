%token	T_SECT_BSS T_SECT_VRAM T_SECT_CODE T_SECT_HOME T_SECT_HRAM

%token	T_Z80_ADC T_Z80_ADD T_Z80_AND
%token	T_Z80_BIT
%token	T_Z80_CALL T_Z80_CCF T_Z80_CP T_Z80_CPL
%token	T_Z80_DAA T_Z80_DEC T_Z80_DI
%token	T_Z80_EI T_Z80_EX
%token	T_Z80_HALT
%token	T_Z80_INC
%token	T_Z80_JP T_Z80_JR
%token	T_Z80_LD
%token	T_Z80_LDI
%token	T_Z80_LDD
%token	T_Z80_LDIO
%token	T_Z80_NOP
%token	T_Z80_OR
%token	T_Z80_POP T_Z80_PUSH
%token	T_Z80_RES T_Z80_RET T_Z80_RETI T_Z80_RST
%token	T_Z80_RL T_Z80_RLA T_Z80_RLC T_Z80_RLCA
%token	T_Z80_RR T_Z80_RRA T_Z80_RRC T_Z80_RRCA
%token	T_Z80_SBC T_Z80_SCF T_Z80_STOP
%token	T_Z80_SLA T_Z80_SRA T_Z80_SRL T_Z80_SUB T_Z80_SWAP
%token	T_Z80_XOR

%token	T_MODE_A T_MODE_B T_MODE_C T_MODE_C_IND T_MODE_D T_MODE_E T_MODE_H T_MODE_L
%token	T_MODE_AF
%token	T_MODE_BC T_MODE_BC_IND
%token	T_MODE_DE T_MODE_DE_IND
%token	T_MODE_SP T_MODE_SP_IND
%token	T_MODE_HL T_MODE_HL_IND T_MODE_HL_INDDEC T_MODE_HL_INDINC
%token	T_CC_NZ T_CC_Z T_CC_NC

%type	<nConstValue>	reg_r
%type	<nConstValue>	reg_ss
%type	<nConstValue>	reg_rr
%type	<nConstValue>	reg_tt
%type	<nConstValue>	ccode
%type	<sVal>			op_a_n
%type	<nConstValue>	op_a_r
%type	<nConstValue>	op_hl_ss
%type	<sVal>			op_mem_ind
