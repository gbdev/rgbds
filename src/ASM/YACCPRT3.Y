%start asmfile

%%

asmfile			:	lines lastline
;

lastline		:	/* empty */
				|	line
					{ nLineNo+=1; nTotalLines+=1; }
;

lines			:	/* empty */
				|	lines line '\n'
					{ nLineNo+=1; nTotalLines+=1; }
;

line			:	/* empty */
				|	label
				|	label cpu_command
				|	label macro
				|	label simple_pseudoop
				|	pseudoop
;

label			:	/* empty */
				|	T_LABEL			{	if( $1[0]=='.' )
											sym_AddLocalReloc($1);
										else
											sym_AddReloc($1);
									}
				|	T_LABEL ':'		{	if( $1[0]=='.' )
											sym_AddLocalReloc($1);
										else
											sym_AddReloc($1);
									}
				|	T_LABEL ':' ':'	{ sym_AddReloc($1); sym_Export($1); }
;

macro			:	T_ID
					{
						yy_set_state( LEX_STATE_MACROARGS );
					}
					macroargs
					{
						yy_set_state( LEX_STATE_NORMAL );

						if( !fstk_RunMacro($1) )
						{
							yyerror( "No such macro" );
						}
					}
;

macroargs		:	/* empty */
				|	macroarg
				|	macroarg ',' macroargs
;

macroarg 		:	T_STRING
					{ sym_AddNewMacroArg( $1 ); }
;

pseudoop		:	equ
				|	set
				|	rb
				|	rw
				|	rl
				|	equs
				|	macrodef
;

simple_pseudoop	:	include
				|	printf
				|	printt
				|	printv
				|	if
				|	else
				|	endc
				|	import
				|	export
				|	global
				|	db
				|	dw
				|	dl
				|	ds
				|	section
				|	rsreset
				|	rsset
				|	incbin
				|	rept
				|	shift
				|	fail
				|	warn
				|	purge
				|	pops
				|	pushs
				|	popo
				|	pusho
				|	opt
;

opt				:	T_POP_OPT
					{
						yy_set_state( LEX_STATE_MACROARGS );
					}
					opt_list
					{
						yy_set_state( LEX_STATE_NORMAL );
					}
;

opt_list		:	opt_list_entry
				|	opt_list_entry ',' opt_list
;

opt_list_entry	:	T_STRING
					{
						opt_Parse($1);
					}
;

popo			:	T_POP_POPO
					{ opt_Pop(); }
;

pusho			:	T_POP_PUSHO
					{ opt_Push(); }
;

pops			:	T_POP_POPS
					{ out_PopSection(); }
;

pushs			:	T_POP_PUSHS
					{ out_PushSection(); }
;

fail			:	T_POP_FAIL string
					{ fatalerror($2); }
;

warn			:	T_POP_WARN string
					{ yyerror($2); }
;

shift			:	T_POP_SHIFT
					{ sym_ShiftCurrentMacroArgs(); }
;

rept			:	T_POP_REPT const
					{
						copyrept();
						fstk_RunRept( $2 );
					}
;

macrodef		:	T_LABEL ':' T_POP_MACRO
					{
						copymacro();
						sym_AddMacro($1);
					}
;

equs			:	T_LABEL T_POP_EQUS string
					{ sym_AddString( $1, $3 ); }
;

rsset			:	T_POP_RSSET const
					{ sym_AddSet( "_RS", $2 ); }
;

rsreset			:	T_POP_RSRESET
					{ sym_AddSet( "_RS", 0 ); }
;

rl				:	T_LABEL T_POP_RL const
					{
						sym_AddEqu( $1, sym_GetConstantValue("_RS") );
						sym_AddSet( "_RS", sym_GetConstantValue("_RS")+4*$3 );
					}
;

rw				:	T_LABEL T_POP_RW const
					{
						sym_AddEqu( $1, sym_GetConstantValue("_RS") );
						sym_AddSet( "_RS", sym_GetConstantValue("_RS")+2*$3 );
					}
;

rb				:	T_LABEL T_POP_RB const
					{
						sym_AddEqu( $1, sym_GetConstantValue("_RS") );
						sym_AddSet( "_RS", sym_GetConstantValue("_RS")+$3 );
					}
;

ds				:	T_POP_DS const
					{ out_Skip( $2 ); }
;

db				:	T_POP_DB constlist_8bit
;

dw				:	T_POP_DW constlist_16bit
;

dl				:	T_POP_DL constlist_32bit
;

purge			:	T_POP_PURGE
					{
						oDontExpandStrings=1;
					}
					purge_list
					{
						oDontExpandStrings=0;
					}
;

purge_list		:	purge_list_entry
				|	purge_list_entry ',' purge_list
;

purge_list_entry	:	T_ID	{ sym_Purge($1); }
;

import			:	T_POP_IMPORT import_list
;

import_list		:	import_list_entry
				|	import_list_entry ',' import_list
;

import_list_entry	:	T_ID	{ sym_Import($1); }
;

export			:	T_POP_EXPORT export_list
;

export_list		:	export_list_entry
				|	export_list_entry ',' export_list
;

export_list_entry	:	T_ID	{ sym_Export($1); }
;

global			:	T_POP_GLOBAL global_list
;

global_list		:	global_list_entry
				|	global_list_entry ',' global_list
;

global_list_entry	:	T_ID	{ sym_Global($1); }
;

equ				:	T_LABEL T_POP_EQU const
					{ sym_AddEqu( $1, $3 ); }
;

set				:	T_LABEL T_POP_SET const
					{ sym_AddSet( $1, $3 ); }
;

include			:	T_POP_INCLUDE string
					{
						if( !fstk_RunInclude($2) )
						{
							yyerror( "File not found" );
						}
					}
;

incbin			:	T_POP_INCBIN string
					{ out_BinaryFile( $2 ); }
;

printt			:	T_POP_PRINTT string
					{
						if( nPass==1 )
							printf( "%s", $2 );
					}
;

printv			:	T_POP_PRINTV const
					{
						if( nPass==1 )
							printf( "$%lX", $2 );
					}
;

printf			:	T_POP_PRINTF const
					{
						if( nPass==1 )
							math_Print( $2 );
					}
;

if				:	T_POP_IF const
					{
						nIFDepth+=1;
						if( !$2 )
						{
							if_skip_to_else();	/* will continue parsing just after ELSE or just at ENDC keyword */
						}
					}

else			:	T_POP_ELSE
					{
						if_skip_to_endc();		/* will continue parsing just at ENDC keyword */
					}
;

endc			:	T_POP_ENDC
					{
						nIFDepth-=1;
					}
;

const_3bit		:	const
					{
						if( ($1<0) || ($1>7) )
						{
							yyerror( "Immediate value must be 3-bit" );
						}
						else
							$$=$1&0x7;
					}
;

constlist_8bit	:	constlist_8bit_entry
				|	constlist_8bit_entry ',' constlist_8bit
;

constlist_8bit_entry	:               { out_Skip( 1 ); }
						|	const_8bit	{ out_RelByte( &$1 ); }
			   			|	string		{ out_String( $1 );  }
;

constlist_16bit	:	constlist_16bit_entry
				|	constlist_16bit_entry ',' constlist_16bit
;

constlist_16bit_entry	:  				{ out_Skip( 2 ); }
						|	const_16bit	{ out_RelWord( &$1 ); }
;


constlist_32bit	:	constlist_32bit_entry
				|	constlist_32bit_entry ',' constlist_32bit
;

constlist_32bit_entry	:				{ out_Skip( 4 ); }
						|	relocconst	{ out_RelLong( &$1 ); }
;


const_PCrel		:	relocconst
					{
						$$ = $1;
						if( !rpn_isPCRelative(&$1) )
							yyerror( "Expression must be PC-relative" );
					}
;

const_8bit		:	relocconst
					{
						if( (!rpn_isReloc(&$1)) && (($1.nVal<-128) || ($1.nVal>255)) )
						{
							yyerror( "Expression must be 8-bit" );
						}
						$$=$1;
					}
;

const_16bit		:	relocconst
					{
						if( (!rpn_isReloc(&$1)) && (($1.nVal<-32768) || ($1.nVal>65535)) )
						{
							yyerror( "Expression must be 16-bit" );
						}
						$$=$1
					}
;


relocconst		:	T_ID
						{ rpn_Symbol(&$$,$1);	$$.nVal = sym_GetValue($1); }
				|	T_NUMBER
						{ rpn_Number(&$$,$1);	$$.nVal = $1; }
				|	string
						{ ULONG r; r=str2int($1); rpn_Number(&$$,r); $$.nVal=r; }
				|	T_OP_LOGICNOT relocconst %prec NEG
						{ rpn_LOGNOT(&$$,&$2); }
				|	relocconst T_OP_LOGICOR relocconst
						{ rpn_LOGOR(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICAND relocconst
						{ rpn_LOGAND(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICEQU relocconst
						{ rpn_LOGEQU(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICGT relocconst
						{ rpn_LOGGT(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICLT relocconst
						{ rpn_LOGLT(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICGE relocconst
					 	{ rpn_LOGGE(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICLE relocconst
					 	{ rpn_LOGLE(&$$,&$1,&$3); }
				|	relocconst T_OP_LOGICNE relocconst
					 	{ rpn_LOGNE(&$$,&$1,&$3); }
				|	relocconst T_OP_ADD relocconst
						{ rpn_ADD(&$$,&$1,&$3); }
				|	relocconst T_OP_SUB relocconst
						{ rpn_SUB(&$$,&$1,&$3); }
				|	relocconst T_OP_XOR relocconst
						{ rpn_XOR(&$$,&$1,&$3); }
				|	relocconst T_OP_OR relocconst
						{ rpn_OR(&$$,&$1,&$3); }
				|	relocconst T_OP_AND relocconst
						{ rpn_AND(&$$,&$1,&$3); }
				|	relocconst T_OP_SHL relocconst
						{ rpn_SHL(&$$,&$1,&$3); }
				|	relocconst T_OP_SHR relocconst
						{ rpn_SHR(&$$,&$1,&$3); }
				|	relocconst T_OP_MUL relocconst
						{ rpn_MUL(&$$,&$1,&$3); }
				|	relocconst T_OP_DIV relocconst
						{ rpn_DIV(&$$,&$1,&$3); }
				|	relocconst T_OP_MOD relocconst
						{ rpn_MOD(&$$,&$1,&$3); }
				|	T_OP_ADD relocconst %prec NEG
						{ $$ = $2; }
				|	T_OP_SUB relocconst %prec NEG
						{ rpn_UNNEG(&$$,&$2); }
				|	T_OP_NOT relocconst %prec NEG
						{ rpn_UNNOT(&$$,&$2); }
				|	T_OP_BANK '(' T_ID ')'
						{ rpn_Bank(&$$,$3); $$.nVal = 0; }
				|	T_OP_DEF '(' T_ID ')'
						{ rpn_Number(&$$,sym_isConstDefined($3)); }
				|	T_OP_FDIV '(' const ',' const ')'			{ rpn_Number(&$$,math_Div($3,$5)); }
				|	T_OP_FMUL '(' const ',' const ')'			{ rpn_Number(&$$,math_Mul($3,$5)); }
				|	T_OP_SIN '(' const ')'			{ rpn_Number(&$$,math_Sin($3)); }
				|	T_OP_COS '(' const ')'			{ rpn_Number(&$$,math_Cos($3)); }
				|	T_OP_TAN '(' const ')'			{ rpn_Number(&$$,math_Tan($3)); }
				|	T_OP_ASIN '(' const ')'			{ rpn_Number(&$$,math_ASin($3)); }
				|	T_OP_ACOS '(' const ')'			{ rpn_Number(&$$,math_ACos($3)); }
				|	T_OP_ATAN '(' const ')'			{ rpn_Number(&$$,math_ATan($3)); }
				|	T_OP_ATAN2 '(' const ',' const ')'	{ rpn_Number(&$$,math_ATan2($3,$5)); }
				|	T_OP_STRCMP '(' string ',' string ')'	{ rpn_Number(&$$,strcmp($3,$5)); }
				|	T_OP_STRIN '(' string ',' string ')'
					{
						char	*p;
  						if( (p=strstr($3,$5))!=NULL )
						{
							rpn_Number(&$$,p-$3+1);
						}
						else
						{
							rpn_Number(&$$,0);
						}
					}
				|	T_OP_STRLEN '(' string ')'		{ rpn_Number(&$$,strlen($3)); }
				|	'(' relocconst ')'
						{ $$ = $2; }
;

const			:	T_ID							{ $$ = sym_GetConstantValue($1); }
				|	T_NUMBER 						{ $$ = $1; }
				|	string						{ $$ = str2int($1); }
				|	T_OP_LOGICNOT const %prec NEG	{ $$ = !$2; }
				|	const T_OP_LOGICOR const		{ $$ = $1 || $3; }
				|	const T_OP_LOGICAND const		{ $$ = $1 && $3; }
				|	const T_OP_LOGICEQU const		{ $$ = $1 == $3; }
				|	const T_OP_LOGICGT const 		{ $$ = $1 > $3; }
				|	const T_OP_LOGICLT const		{ $$ = $1 < $3; }
				|	const T_OP_LOGICGE const 		{ $$ = $1 >= $3; }
				|	const T_OP_LOGICLE const 		{ $$ = $1 <= $3; }
				|	const T_OP_LOGICNE const 		{ $$ = $1 != $3; }
				|	const T_OP_ADD const			{ $$ = $1 + $3; }
				|	const T_OP_SUB const			{ $$ = $1 - $3; }
				|	T_ID  T_OP_SUB T_ID				{ $$ = sym_GetDefinedValue($1) - sym_GetDefinedValue($3); }
				|	const T_OP_XOR const			{ $$ = $1 ^ $3; }
				|	const T_OP_OR const				{ $$ = $1 | $3; }
				|	const T_OP_AND const			{ $$ = $1 & $3; }
				|	const T_OP_SHL const			{ $$ = $1 << $3; }
				|	const T_OP_SHR const			{ $$ = $1 >> $3; }
				|	const T_OP_MUL const			{ $$ = $1 * $3; }
				|	const T_OP_DIV const			{ $$ = $1 / $3; }
				|	const T_OP_MOD const			{ $$ = $1 % $3; }
				|	T_OP_ADD const %prec NEG		{ $$ = +$2; }
				|	T_OP_SUB const %prec NEG		{ $$ = -$2; }
				|	T_OP_NOT const %prec NEG		{ $$ = 0xFFFFFFFF^$2; }
				|	T_OP_FDIV '(' const ',' const ')'			{ $$ = math_Div($3,$5); }
				|	T_OP_FMUL '(' const ',' const ')'			{ $$ = math_Mul($3,$5); }
				|	T_OP_SIN '(' const ')'			{ $$ = math_Sin($3); }
				|	T_OP_COS '(' const ')'			{ $$ = math_Cos($3); }
				|	T_OP_TAN '(' const ')'			{ $$ = math_Tan($3); }
				|	T_OP_ASIN '(' const ')'			{ $$ = math_ASin($3); }
				|	T_OP_ACOS '(' const ')'			{ $$ = math_ACos($3); }
				|	T_OP_ATAN '(' const ')'			{ $$ = math_ATan($3); }
				|	T_OP_ATAN2 '(' const ',' const ')'	{ $$ = math_ATan2($3,$5); }
				|	T_OP_DEF '(' T_ID ')'				{ $$ = sym_isConstDefined($3); }
				|	T_OP_STRCMP '(' string ',' string ')'	{ $$ = strcmp( $3, $5 ); }
				|	T_OP_STRIN '(' string ',' string ')'
					{
						char	*p;
  						if( (p=strstr($3,$5))!=NULL )
						{
							$$ = p-$3+1;
						}
						else
						{
							$$ = 0;
						}
					}
				|	T_OP_STRLEN '(' string ')'		{ $$ = strlen($3); }
				|	'(' const ')'					{ $$ = $2; }
;

string			:	T_STRING
					{ strcpy($$,$1); }
				|	T_OP_STRSUB '(' string ',' const ',' const ')'
					{ strncpy($$,$3+$5-1,$7); $$[$7]=0; }
				|	T_OP_STRCAT '(' string ',' string ')'
					{ strcpy($$,$3); strcat($$,$5); }
				|	T_OP_STRUPR '(' string ')'
					{ strcpy($$,$3); strupr($$); }
				|	T_OP_STRLWR '(' string ')'
					{ strcpy($$,$3); strlwr($$); }
;