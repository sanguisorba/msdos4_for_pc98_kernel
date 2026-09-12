; New PC-98 BIOS entry and DOS request adapters. GPL-2.0-or-later.
CPU 8086
%include "segs.inc"
%include "stacks.inc"
%define IN_KERNEL_ASM
%include "nec98cfg.inc"

segment PSP
..start:
global start
start:
    jmp       entry_code
entry                       equ start
%include "pc98_lowmem.inc"
    times     0x2d00-($-$$) db 0

; Establish DGROUP and a private boot stack before calling C.
entry_code:
    cli
    cld
    mov       ax,DGROUP
    mov       ds,ax
    mov       es,ax
    mov       ss,ax
    mov       sp,stack_top

; Round the end of SYSINIT up to a paragraph boundary.
; Load DOS beyond that boundary before transferring control to SYSINIT.
    mov       ax,seg SYSSIZE
    mov       bx,SYSSIZE
    add       bx,15
    mov       cl,4
    shr       bx,cl
    add       ax,bx
    mov       [_final_dos_seg],ax
    mov       [_bios_end_seg],ax
    xor       ax,ax
    mov       es,ax

; preserve BIOS service vectors, installing only narrowly handled functions
    mov       ax,[es:15h*4]
    mov       [cs:old15],ax
    mov       ax,[es:15h*4+2]
    mov       [cs:old15+2],ax
    mov       word [es:15h*4],int15shim
    mov       [es:15h*4+2],cs
    mov       ax,[es:6*4]
    mov       [cs:old6],ax
    mov       ax,[es:6*4+2]
    mov       [cs:old6+2],ax
    mov       word [es:6*4],stop_key
    mov       [es:6*4+2],cs
    mov       word [es:5*4],noop_int
    mov       [es:5*4+2],cs
    mov       word [es:2ah*4],noop_int
    mov       [es:2ah*4+2],cs
    mov       word [es:28h*4],noop_int
    mov       [es:28h*4+2],cs
    mov       word [es:29h*4],reloc_call_int29_handler
    mov       word [es:29h*4+2],seg reloc_call_int29_handler
    mov       word [es:0dch*4],reloc_call_intdc_handler
    mov       word [es:0dch*4+2],seg reloc_call_intdc_handler

; Initialize the imported runtime before enabling interrupts.
    call      seg  crt_gate:crt_gate            ; Adapt the far boot call to the runtime near entry point.

    mov       ax,DGROUP
    mov       ds,ax
    mov       es,ax
    sti
    call      seg  main_gate:main_gate          ; Adapt the far boot call to the Watcom C near-call convention.

; Pass documented MS-DOS/SYSINIT fields, not instruction patches.
    mov       ax,seg SYSINIT
    mov       es,ax
    mov       word [es:DEVICE_LIST],_con_dev
    mov       word [es:DEVICE_LIST+2],seg _con_dev
    mov       ax,[_final_dos_seg]
    mov       [es:CURRENT_DOS_LOCATION],ax
    mov       [es:FINAL_DOS_LOCATION],ax
    mov       ax,[_boot_unit]
    inc       ax
    mov       [es:DEFAULT_DRIVE],al
    mov       ax,[_memory_paras]
    mov       [es:MEMORY_SIZE],ax

; SYSINIT is built with STACKSW=FALSE; no hardware-stack fields exist.
    jmp       seg SYSINIT:SYSINIT
noop_int:
    iret

; COPY is a no-op, as in FreeDOS(98). STOP injects Ctrl-C; Shift-STOP Ctrl-S.
old6                        dd 0

; Identify software INT 6 from the saved return address.
; Forward other uses of vector 6 to the original handler.
stop_key:
    push      bp
    mov       bp,sp
    push      ax
    push      bx
    push      ds
    push      es
    les       bx,[ss:bp+2]
    cmp       word [es:bx-2],06cdh
    jne       .chain
    mov       ax,PGROUP
    mov       ds,ax
    cmp       byte [_in_processing_stopkey],0
    jne       .done
    mov       byte [_in_processing_stopkey],1
    xor       ax,ax
    mov       es,ax
    mov       bl,13h
    test      byte [es:053ah],1
    jnz       .inject
    mov       byte [_fd98_retract_hd_pending],1
    mov       al,[_clear_attr]
    mov       [_put_attr],al
    mov       byte [_cursor_view],1
    mov       ax,1101h
    int       18h
    mov       bl,3

; Replace pending keys with one control byte in the segment-0060h
; console input workspace. The offsets below are fixed by that layout.
.inject:
    call      far _nec98_flush_bios_keybuf
    mov       [00c0h],bl
    mov       byte [0103h],1
    mov       word [0104h],00c0h
    mov       byte [_in_processing_stopkey],0
.done:
    pop       es
    pop       ds
    pop       bx
    pop       ax
    pop       bp
    iret
.chain:
    pop       es
    pop       ds
    pop       bx
    pop       ax
    pop       bp
    jmp       far [cs:old6]
extern _nec98_flush_bios_keybuf
old15                       dd 0

; Provide the two IBM-style queries used by SYSINIT; chain other functions.
int15shim:
    cmp       ah,0c0h
    je        .config
    cmp       ah,88h
    je        .mem
    jmp       far [cs:old15]
.config:
    push      cs
    pop       es
    mov       bx,romconfig
    xor       ah,ah
    jmp       short .ok
.mem:
    xor       ax,ax
.ok:
    push      bp
    mov       bp,sp

; Clear carry in the saved interrupt frame so IRET returns success.
    and       word [ss:bp+6],0fffeh
    pop       bp
    iret

; PCjr model selects SYSINIT's explicit no-IBM-stack/no-global-rearm paths.
; This is only the private compatibility response; it does not alter PC-98 ROM.
romconfig                   dw 8
    db        0fdh,0,0,0,0,0,0,0

extern _con_dev,GenStrategy,ConIntr,_nul_intr
extern reloc_call_int29_handler,reloc_call_intdc_handler
extern _memory_paras

segment _DATA
global _final_dos_seg,_bios_end_seg,_boot_unit,_bpb_table
_final_dos_seg              dw 0
_bios_end_seg               dw 0
_boot_unit                  dw 0

; BPB offsets passed to SYSINIT in the driver data segment.
_bpb_table                  times 26 dw 0

; Downward-growing boot stack, independent of SYSINIT STACKSW.
    times     4096 db 0
stack_top:

segment INIT_TEXT
extern init_crt
crt_gate:
    call      init_crt
    retf

segment HMA_TEXT
extern bios_main_
main_gate:
    call      bios_main_
    retf
global PUTCH98,KEY98,HALT98

; Pascal word argument is at BP+4. Preserve AX and discard the argument.
PUTCH98:
    push      bp
    mov       bp,sp
    push      ax
    mov       al,[bp+4]
    int       29h
    pop       ax
    pop       bp
    ret       2
KEY98:
    xor       ah,ah
    int       18h
    ret

; Terminal boot-failure path; interrupts remain disabled.
HALT98:
    cli
    hlt
    jmp       HALT98

; Only symbols required by the unchanged Microsoft SYSINIT objects.

segment CODE class=code
global RE_INIT,INT19SEM,MULTRK_FLAG,EC35_FLAG,KEYRD_FUNC,KEYSTS_FUNC
RE_INIT:
    retf
INT19SEM                    db 0
MULTRK_FLAG                 dw 0
EC35_FLAG                   db 0
KEYRD_FUNC                  db 0
KEYSTS_FUNC                 db 1
%assign v 0
%rep 14
%assign v v+1
%endrep

; Compatibility storage exported for Microsoft SYSINIT references.
%macro OLD 1
global INT19OLD%1
INT19OLD%1                  dd 0
%endmacro
    OLD       02
    OLD       08
    OLD       09
    OLD       0A
    OLD       0B
    OLD       0C
    OLD       0D
    OLD       0E
    OLD       70
    OLD       72
    OLD       73
    OLD       74
    OLD       76
    OLD       77
extern SYSINIT,SYSSIZE,DEVICE_LIST,CURRENT_DOS_LOCATION,FINAL_DOS_LOCATION
extern DEFAULT_DRIVE,MEMORY_SIZE

segment HMA_TEXT

; Unsigned 32-bit arithmetic helpers required by Watcom C.
%include "ludivmul.inc"
global __U4M,__U4D
__U4M:
    LMULU
__U4D:
    LDIVMODU
