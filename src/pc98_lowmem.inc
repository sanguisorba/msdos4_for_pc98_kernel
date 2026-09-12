; IO-facing fixed layout derived from FreeDOS(98) kernel.asm, GPL-2.0-or-later.
; Original copyright: Pasquale J. Villani, 1995-1996; see vendor/freedos/COPYING.
; Included in the PSP segment loaded at 0060h. TIMES padding preserves
; offsets accessed directly by imported IO code. Do not compact these gaps.
    times     0020h-($-entry) db 0

; Keep product/revision bytes zero; no proprietary product ID is claimed.
global  _product_no
_product_no                 dw  0               ; 0020h product number
global	_inside_revision
_inside_revision            db  0               ; 0022h internal revision

    times     0031h - ($ - entry) db 0
global  _promem_under16
_promem_under16             db  0               ; 0031h protected-memory size below 16 MB, in 128 KB units
;times   0032h - ($ - entry) db 0
global  _text_vram_segment
_text_vram_segment          dw 0a000h           ; 0032h segment of Text RAM

    times     0068h - ($ - entry) db 0
global  _auxspeed
_auxspeed                   db  0, 0            ; 0068h copy of MEMSW1,2: aux (com0) configuration

    times     006ch - ($ - entry) db 0

; bios_main replaces this initial list with enumerated DA/UA identifiers.
global  _daua_list
_daua_list                  db  80h             ; A: (SASI/IDE HD)

; db 90h ; B: (FD)
; db 91h ; C: (FD)

    times     008ah - ($ - entry) db 0
global  _kanjigraph_mode
_kanjigraph_mode            db  1               ; 008ah Kanji/graph mode
global  _kanjigraph_char
_kanjigraph_char            db ' '              ; 008bh lower-left character in Kanji/graph mode
global	_shiftfunc_char
_shiftfunc_char             db  ' '             ; 008ch lower-left character in normal/shift function-key display

    times     00a4h - ($ - entry) db 0
global _in_processing_stopkey
_in_processing_stopkey      db  0               ; 00a4h semaphore for STOP key (in int6 handler)

    times     0110h - ($ - entry) db 0
global  _cursor_y
_cursor_y                   db  0               ; 0110h cursor row
global  _function_flag
_function_flag              db  0               ; 0111h function-key display state
global  _scroll_bottom
_scroll_bottom              db  25 - 1          ; 0112h bottom scroll row
global  _crt_line
_crt_line                   db  1               ; 0113h screen line mode
global  _clear_attr
_clear_attr                 db  0e1h            ; 0114h erase attribute
global  _kanji2_wait
_kanji2_wait                db  0               ; 0115h waiting for second Kanji byte
global  _kanji1_code
_kanji1_code                db  0               ; 0116h first Kanji byte

    times     0119h - ($ - entry) db 0
global  _clear_char
_clear_char                 db  ' '             ; 0119h erase character

    times     011bh - ($ - entry) db 0
global  _cursor_view
_cursor_view                db  1               ; 011bh cursor visibility
global  _cursor_x
_cursor_x                   db  0               ; 011ch cursor column
global  _put_attr
_put_attr                   db  0e1h            ; 011dh output attribute
global  _scroll_top
_scroll_top                 db  0               ; 011eh top scroll row
global  _scroll_wait
_scroll_wait                dw  1               ; 011fh scroll delay

    times     0126h - ($ - entry) db 0
global  _save_cursor_y
_save_cursor_y              db  0               ; 0126h saved cursor row
global  _save_cursor_x
_save_cursor_x              db  0               ; 0127h saved cursor column

    times     012bh - ($ - entry) db 0
global  _save_cursor_attr
_save_cursor_attr           db  0e1h            ; 012bh saved cursor attribute

;times   012ch - ($ - entry) db 0
global  _esc_seq_cursor_pos
_esc_seq_cursor_pos         db  1bh, '[24;80R'  ; 012ch - 0133h scratchpad for esc[6n ret

    times     0136h - ($ - entry) db 0
global  _disk_last_access_unit
_disk_last_access_unit      db 0                ; 0136h - last accessed disk unit number

;               times   256 - ($ - entry) db 0


%if 1
    times     07ffh - ($ - entry) db 0
global  _fd98_retract_hd_pending
_fd98_retract_hd_pending    db 0                ; a temporary solution: not compatible with MS-DOS

%endif

; some codes and data in IO.SYS area
;--------
; Imported keyboard/console routines below must reside in segment 0060h.
%ifdef INCLUDE_CONKEY60
    align     2
global	conkey60_begin
global	conkey60_end
conkey60_begin:
%include "conkey60.asm"
conkey60_end:
%endif ; INCLUDE_CONKEY60

%ifdef INCLUDE_CONSEG60
    align     2
global	conseg60_begin
global	conseg60_end
conseg60_begin:
%include "conseg60.asm"
conseg60_end:
%endif ; INCLUDE_CONSEG60
;--------


; reserved area for RSDRV
;--------
    times     17e0h - ($ - entry) db 0

    times     17eeh - ($ - entry) db 0
global _aux1speed, _aux2speed
_aux1speed                  db  0, 0            ; 17EE aux1 (com1) configuration
_aux2speed                  db  0, 0            ; 17F0 aux2 (com2) configuration

    times     1802h - ($ - entry) db 0
global _rsdrv_seg
_rsdrv_seg                  dw  0               ; 1802 segment of RSDRV.SYS

    times     1820h - ($ - entry) db 0
;--------


; some codes and data in IO.SYS area
;--------
; Imported support routines share the fixed-segment environment.
%define INCLUDE_SUPSEG60
    align     2
global	supseg60_begin
global	supseg60_end
supseg60_begin:
%include "supseg60.asm"
supseg60_end:
;--------

%ifdef DUMMY_CON_IN_IOSYS

; dummy con driver (workaround for some FEP driver(s) - yes, ATOK6 it is)
; On genuine (NEC's and EPSON's) MS-DOSes, CON device is placed at:
;   0060:2660  NEC MS-DOS 2.11
;   0060:3400  MS-DOS 3.3, 5.0, 6.2 (and up to Win98SE)

; anywhere at seg 60h ... temporary here
    times     2400h - ($ - entry) db 0
_dummy_con_dev              equ     $
    dw        _dummy_prn_dev, seg _dummy_prn_dev
    dw        8013h                             ; chardev (stdin, stdout, int29h)
    dw        _dummy_strategy
    dw        _dummy_con_intr
    db        'CON     '
_dummy_prn_dev              equ     $
    dw        0ffffh, 0ffffh
    dw        8000h
    dw        _dummy_strategy
    dw        _dummy_prn_intr
    db        'PRN     '

_dummy_strategy:
extern GenStrategy
    jmp       far GenStrategy
_dummy_con_intr:
extern ConIntr
    jmp       far ConIntr
_dummy_prn_intr:
extern NulIntr
    jmp       far _nul_intr
%endif ; DUMMY_CON_IN_IOSYS

; Independent of SYSINIT STACKSW. Each stack has 256 bytes and grows
; downward from the exported initial SP. Wrappers restore the caller stack.
%ifdef USE_PRIVATE_INT29_STACK		; (see entry.asm)

; private stack for int29
global int29_stack_bottom
    align     2
    times     16 db 'Stack for int29.'          ; 16x16 = 256bytes
int29_stack_bottom:
%endif
%ifdef USE_PRIVATE_INTDC_STACK		; (see entry.asm)

; private stack for intdc
global intdc_stack_bottom
    align     2
    times     16 db 'Stack for intDC.'          ; 16x16 = 256bytes
intdc_stack_bottom:
%endif

