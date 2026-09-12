%ifndef NEC98CFG_ASM_DEFINED
%define NEC98CFG_ASM_DEFINED 1

;-------------------------------
%define DUMMY_CON_IN_IOSYS  1
%define INT6_HANDLER_IN_IOSYS 1
%define UNHANDLED_INT_HANDLER_IN_IOSYS 1
%define BIG_SECTOR 1
%define USE_PRIVATE_INT29_STACK 1
%define USE_PRIVATE_INTDC_STACK 1
%define USE_PUTCRT_SEG60 1
;-------------------------------
%ifdef IN_KERNEL_ASM
	%define INCLUDE_CONSEG60 1
	%define INCLUDE_CONKEY60 1
	%define INCLUDE_SUPSEG60 1
%endif
;-------------------------------



%define FAR_CON_DGROUP		PGROUP
%define FAR_CON_TEXTSEG		PSP
%define FAR_CON_TGROUP		PGROUP

%define IOCODE_DGROUP PGROUP
%define IOCODE_TEXTSEG PSP
%define IOCODE_TGROUP PGROUP

%macro  arg_f 1-*
	%assign .argloc 6
	%rep  %0
		%ifdef PASCAL
			%rotate -1
		%endif
		definearg %1
		%ifdef STDCALL
			%rotate 1
		%endif
	%endrep
%endmacro


%endif

