; IO adapters for unmodified FreeDOS(98) drivers and Microsoft SYSINIT.
; Interrupt wrappers derived from FreeDOS(98) entry.asm, GPL-2.0-or-later.
; Original copyright Pasquale J. Villani; see vendor/freedos/COPYING.
%include "segs.inc"
%include "stacks.inc"
%include "nec98cfg.inc"

segment HMA_TEXT
extern _int29_main,_intdc_main

; Make DGROUP accessible through CS before replacing the interrupted DS.
bridge_dgroup               dw DGROUP
global reloc_call_int29_handler,reloc_call_intdc_handler
%ifdef USE_PRIVATE_INT29_STACK
extern int29_stack_bottom
global int29_stack_org                          ; just for debugging
global int29_stack_count                        ; ditto

    align     2
int29_stack_org:
    dd        0
int29_stack_count:
    db        0
%endif
%ifdef USE_PRIVATE_INTDC_STACK
extern intdc_stack_bottom
global intdc_stack_org                          ; just for debugging
global intdc_stack_count                        ; ditto

%ifndef USE_PRIVATE_INT29_STACK
    align     2
%endif
intdc_stack_count:
    db        0
intdc_stack_org:
    dd        0
%endif

; AL contains the output byte. Preserve registers around the C handler.
; The optional stack switches only on the outermost call: 00h -> FFh.
; The matching FFh -> 00h transition restores the original SS:SP.
reloc_call_int29_handler:
%ifdef USE_PRIVATE_INT29_STACK
    cli
    sub       byte [cs: int29_stack_count], 1
    jnc       .stk_set
    mov       word [cs: int29_stack_org], sp
    mov       word [cs: int29_stack_org + 2], ss
    mov       sp, PSP                           ; 0060h
    mov       ss, sp
    mov       sp, int29_stack_bottom
.stk_set:
%endif
    cld
    sti
    Protect386Registers
    push      bx
    push      cx
    push      dx
    push      ds
    push      es
    push      ax
    mov       ds, [cs: bridge_dgroup]
    call      _int29_main
    pop       ax
    pop       es
    pop       ds
    pop       dx
    pop       cx
    pop       bx
    Restore386Registers
%ifdef USE_PRIVATE_INT29_STACK
    cli
    add       byte [cs: int29_stack_count], 1
    jnc       .re_stk
    mov       ss, word [cs: int29_stack_org + 2]
    mov       sp, word [cs: int29_stack_org]
.re_stk:
;sti
%endif
    iret

;
; int dch entry point
;
;       VOID INRPT far
;       intdc_handler(iregs UserRegs)
;
; PUSH$ALL builds the register frame expected by intdc_main. Pass SS:BP
; as a far pointer so C can inspect/update the saved registers.
reloc_call_intdc_handler:
%ifdef USE_PRIVATE_INTDC_STACK
    cli
    sub       byte [cs: intdc_stack_count], 1
    jnc       .stk_set
    mov       word [cs: intdc_stack_org + 2], ss
    mov       word [cs: intdc_stack_org], sp
    mov       sp, PSP                           ; 0060h
    mov       ss, sp
    mov       sp, intdc_stack_bottom
.stk_set:
%endif
    cld
    sti
    PUSH$ALL
    mov       bp,sp
    Protect386Registers
    mov       ds, [cs: bridge_dgroup]
    push      ss
    push      bp
    call      _intdc_main
    pop       ax
    pop       ax
    Restore386Registers
    POP$ALL
%ifdef USE_PRIVATE_INTDC_STACK
    cli
    add       byte [cs: intdc_stack_count], 1
    jnc       .re_stk
    mov       ss, word [cs: intdc_stack_org + 2]
    mov       sp, word [cs: intdc_stack_org]
.re_stk:
;sti
%endif
    iret

segment _IO_TEXT
global _TEXT_DGROUP
_TEXT_DGROUP                dw DGROUP
extern _ReqPktPtr
extern _bios_end_seg
global _nul_intr

; Complete the request saved by GenStrategy. INIT returns the resident
; end; INPUT returns zero bytes. Status 0100h means DONE.
_nul_intr:
    push      ax
    push      ds
    push      es
    push      bx
    mov       ax,LGROUP
    mov       es,ax
    les       bx,[es:_ReqPktPtr]
    cmp       byte [es:bx+2],0
    jne       .read
    mov       byte [es:bx+13],0
    mov       word [es:bx+14],0
    mov       ax,DGROUP
    mov       ds,ax
    mov       ax,[_bios_end_seg]
    mov       [es:bx+16],ax
.read:
    cmp       byte [es:bx+2],4
    jne       .done
    mov       word [es:bx+18],0
.done:
    mov       word [es:bx+3],100h
    pop       bx
    pop       es
    pop       ds
    pop       ax
    retf

segment _DATA

; io.asm calls these trampolines through DGROUP.
global _reloc_call_blk_driver,_reloc_call_clk_driver
extern _bios_disk,_bios_clock

; Far tail jumps preserve the device caller return address for C adapters.
_reloc_call_blk_driver:
    jmp       far _bios_disk
_reloc_call_clk_driver:
    jmp       far _bios_clock
    align     2

; Separate downward-growing stacks for block and clock requests.
    times     4096 db 0
global blk_stk_top
blk_stk_top:
    times     1024 db 0
global clk_stk_top
clk_stk_top:
