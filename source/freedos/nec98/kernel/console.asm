;
; File:
;                          console.asm
; Description:
;                      Console device driver
;
;                       Copyright (c) 1998
;                       Pasquale J. Villani
;                       All Rights Reserved
;
; This file is part of DOS-C.
;
; DOS-C is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version
; 2, or (at your option) any later version.
;
; DOS-C is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
; the GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public
; License along with DOS-C; see the file COPYING.  If not,
; write to the Free Software Foundation, 675 Mass Ave,
; Cambridge, MA 02139, USA.
;
; $Header: /cvsroot/freedos/kernel/kernel/console.asm,v 1.17 2004/05/23 22:04:42 bartoldeman Exp $
;

                %include "io.inc"
                %include "stacks.inc"
                %include "nec98cfg.inc"

                %include "conkey60.asm"
                %include "conseg60.asm"

segment	_IO_FIXED_DATA

                global  ConTable
ConTable        db      0Ah
                dw      _IOExit		; 00h init
                dw      _IOExit		; 01h
                dw      _IOExit		; 02h
                dw      _IOCommandError	; 03h ioctl input
                dw      ConRead		; 04h input
                dw      CommonNdRdExit	; 05h nondestructive input, no wait
                dw      ConInStat	; 06h input status
                dw      ConInpFlush	; 07h input flush
                dw      ConWrite	; 08h output
                dw      ConWrite	; 09h output with verify
                dw      _IOExit		; 0ah output status

segment	_LOWTEXT

seg_0060  dw 0060h

; 0000:xxxx
%define KB_BUF_TOP    0502h
%define KB_BUF_BOTTOM 0522h
%define KB_BUF_SIZEOF (KB_BUF_BOTTOM - KB_BUF_TOP)
%define KB_BUF_HEAD   0524h
%define KB_BUF_TAIL   0526h
%define KB_COUNT      0528h

; 0060:xxxx
%define CON_BUF_COUNT 0103h
%define CON_BUF_HEAD  0104h

; int 18h ah=1
; check keyboard buffer WITHOUT BIOS call
; note: Some FEPs (addtional CON driver) like ATOK will go wrong 
;       by peeking a keystroke via BIOS call)
keybios_peek:
%if 1
    push si
    push ds
    xor si, si
    mov ds, si
    mov bh, byte [KB_COUNT]
    add bh, 0ffh
    sbb bh, bh
    and bh, 1
    jz .exit
    mov si, word [KB_BUF_HEAD]
    lodsw                       ; assume DF=0 (cld)
  .exit:
    pop ds
    pop si
    ret
%else
    mov ah, 1
    int 18h
    ret
%endif

; to work some FEPs correctly, need real BIOS call...
keybios_remove:
    mov ah, 0
    int 18h
    ret


; assume DF=0 (cld)

; in cx = 0 peek a key
;         1 peek and remove a key from conbuf (if exist)
; result
;    ZF   0 = a key, 1 = no key
;         al = keycode
peekget_from_conin_buf_sub:
    push si
    push ds
    mov ds, [cs: seg_0060]
    mov si, word [CON_BUF_HEAD]
    or si, si
    jz .exit        ; zf=1 if no data (al = 0)
    mov al, byte [CON_BUF_COUNT]
    or al, al
    jz .exit        ; zf=1 if no data
    mov al, byte [si]
    add si, cx
    mov word [CON_BUF_HEAD], si
    sub byte [CON_BUF_COUNT], cl
    or al, al     ; zf=0 if data
  .exit:
    pop ds
    pop si
    ret

flush_conin_buf:
    push ds
    mov ds, [cs: seg_0060]
    mov byte [CON_BUF_COUNT], 0
    mov word [CON_BUF_HEAD], 0
    pop ds
    ret

flush_bios_keybuf:
    push ax
    push ds
    mov ax, 0
    mov ds, ax
    mov byte [KB_COUNT], al
    mov ax, KB_BUF_TOP
    mov word [KB_BUF_HEAD], ax
    mov word [KB_BUF_TAIL], ax
    pop ds
    pop ax
    ret


check_fkey:
    push dx
    push ds
    mov dx, ax
    mov ax, FAR_CON_DGROUP
    mov ds, ax
    mov si, _cnvkey_src
  .lp:
    lodsw
    or ax, ax
    jz .not_match
    cmp ax, dx
    jne .lp
    sub si, _cnvkey_src + 2
    add si, si
    add si, si
    add si, si
    add si, _cnvkey_dest  ; si = keystr_ptr and ZF=0
    cmp byte [si], 0
    jne .exit
  .not_match:
    sub si, si            ; si = 0 and ZF=1
  .exit:
    mov ax, dx
    pop ds
    pop dx
    ret

copy_fkey_to_conbuf:
    push ax
    push cx
    push di
    push ds
    push es
    pushf
    mov es, [cs: seg_0060]
    mov ax, FAR_CON_DGROUP
    mov ds, ax
    xor cx, cx
    mov di, 00c0h
    mov byte [es: CON_BUF_COUNT], cl  ; buf len=0 (for a proof)
    mov word [es: CON_BUF_HEAD], di
  .lp:
    lodsb
    or al, al
    jz .exit
    stosb
    inc cl
    cmp cl, 16
    jb .lp
  .exit:
    mov byte [es: CON_BUF_COUNT], cl
    popf
    or cl, cl
    pop es
    pop ds
    pop di
    pop cx
    pop ax
    ret

ndwait_sub:
    push bx
    push cx
    push dx
    push si
    cli
    call peekget_from_conin_buf_sub
    jnz .exit                     ; ZF=0 if key(s) in conbuf
  .peekget_key:
    call keybios_peek
    or bh, bh
    jz .exit                      ; ZF=1 (no key in BIOS keybuf)
    
    cmp ax, 1a00h   ; ctrl-@
    je .realkey
    or al, al
    jne .realkey
    ; special keys
    call keybios_remove
    call check_fkey
    jz .keyconv_exit
    call copy_fkey_to_conbuf
  .keyconv_exit:
    xor ax, ax    ; ZF=1
    jmp short .exit

  .realkey:
    jcxz .realkey_2
    call keybios_remove
  .realkey_2:
    or ax, ax     ; ZF=0
  .exit:
    sti
    pop si
    pop dx
    pop cx
    pop bx
    ret


push_fkey:
    push dx
    push si
    push di
    push ds
    push es
    mov es, [cs: seg_0060]
    mov dx, ax
    mov ax, FAR_CON_DGROUP
    mov ds, ax
    mov si, _cnvkey_src
  .lp:
    lodsw
    or ax, ax
    jz .exit      ; no_conv zf=1
    cmp ax, dx
    jne .lp
  ; conv
    sub si, _cnvkey_src + 2
    add si, si
    add si, si
    add si, si
    add si, _cnvkey_dest
    mov di, 00c0h           ; use keybuf 0060:00c0
    mov byte [es: CON_BUF_COUNT], 0  ; buf len=0 (for a proof)
    mov word [es: CON_BUF_HEAD], di
    push cx
    xor cx, cx
  .lp_cpybuf:
    lodsb
    or al, al
    jz .l2
    stosb
    inc cx
    cmp cx, 16
    jb .lp_cpybuf
  .l2:
    mov byte [es: CON_BUF_COUNT], cl
%if 1                        ; temporary fix
    or di, di               ; always zf=0
%else
    or cl, cl               ; zf=0 if cl > 0
%endif
    pop cx
  .exit:
    pop es
    pop ds
    pop di
    pop si
    mov ax, dx
    pop dx
    ret


; 06h input status
    global ConInStat
ConInStat:
    jmp	_IOExit       ; just return

; 04h input
;    global ConRead
ConRead:
  jcxz .do_ioexit
; push ds
; db 68h, 00h, 0a0h
; pop ds
; inc byte [0]
; pop ds
  .lp1:
    push cx
    mov cx, 1
    call ndwait_sub
    pop cx
    jz .lp1
    stosb
    loop .lp1
  .do_ioexit:
    jmp	_IOExit       ; just return

; 05h nondestructive input, no wait
    global CommonNdRdExit
CommonNdRdExit:
    push cx
    xor cx, cx
    call ndwait_sub
    pop cx
    jz .no_data
    jmp short .set_data
  .set_data:
    lds bx, [cs:_ReqPktPtr]
    mov [bx + 0dh], al
    jmp _IOExit

  .no_data:
    jmp	_IODone

; 07h input flush
    global ConInpFlush
ConInpFlush:
    cli
    call flush_conin_buf
    call flush_bios_keybuf
    sti
  .loc_end:
    jmp _IOExit

; 08h output
; 09h output with verify
    global ConWrite
ConWrite:
    jcxz	.end
  .lp:
    mov	al, [es:di]
    inc di
    int 29h
    loop .lp
.end:
    jmp	_IOExit


    global _nec98_flush_bios_keybuf
_nec98_flush_bios_keybuf:
    call flush_bios_keybuf
    retf


; for int29

CRT_STS_FLAG	equ	053ch

;	extern	_int29_handler
	extern	reloc_call_int29_handler

segment INIT_TEXT

		global	init_crt
init_crt:
		mov	ah, 0bh		; get crt mode
		int	18h
		test	al, 80h
		jz	.setmode
		push	ax
		mov	ah, 42h		; set graph mode
		mov	ch, 0c0h	; 640x400 color
		int	18h
		pop	ax
	.setmode:
		and	al, 0f9h	; bit2=0 (vertical line), bit1=0 (80columns)
		mov	ah, 0ah		; set crt mode
		int	18h
		mov	ah, 0ch	; show screen
		int	18h
		mov	ah, 11h	; view cursor
		int	18h

	%ifdef INHERIT_CURSOR_POSITION
		pushf
		cli
	.loop1:
		in	al, 60h
		test	al, 04h
		jz	.loop1

		mov	dx, 62h
		mov	al, 0e0h
		out	dx, al	; CSRR

	.loop2:
		in	al, 60h
		test	al, 01h
		jz	.loop2

		in	al, dx	; low addr
		mov	cl, al
		in	al, dx	; high addr
		mov	ch, al
		in	al, dx	; skip
		in	al, dx	; skip
		in	al, dx	; skip
		popf
	%else
		mov	cx, 0
	%endif

		xor	ax, ax
		mov	ds, ax
		test	byte [0501h], 8
		mov	al, 60h			; ax = 0060h
		mov	ds, ax
		mov	ax, 0a000h		; normal (A000)
		jz	.set_vram
		mov	ah, 0e0h		; hireso (E000)
	.set_vram:
		mov	[_text_vram_segment], ax
		mov	ax, cx
		mov	dl, 80
		div	dl
		mov	[_cursor_y], al
		mov	[_cursor_x], ah
		mov	ah, 0bh
		int	18h
		test	al, 1
		jz	.l2
		mov	byte [_scroll_bottom], 20 - 1
		mov	byte [_crt_line], al		; 0 (20rows)
	.l2:
		push	si
		push	ds
		mov	es, [_text_vram_segment]
		push	ds		; xchg ds, es
		push	es
		pop	ds
		pop	es
		mov	si, 3fe2h
		mov	di, 68h
		movsb			; MEM SW1 (A000:3FE2) -> 0060:0068
		add	si, byte 3
		movsb			; MEM SW2 (A000:3FE6) -> 0060:0069
		add	si, byte 3
		lodsb			; MEM SW3 (A000:3FEA)
		stosb			; -> 0060:006A
		add	si, byte 3
		movsb			; MEM SW4 (A000:3FEE) -> 0060:006B
		add	si, byte 3
		test	al, 40h		; check MEMSW3 bit6 (1:Green Monitor Compatible)
		lodsb			; MEM SW5 (A000:3FF2)
		mov	ah, byte [si + 3]	; MEM SW6 (A000:3FF6)
		pop	ds
		mov	word [008dh], ax	; -> 0060:008D, 008E
		jz	.l3
		mov	al, 81h
		mov	[_clear_attr], al
		mov	[_put_attr], al
	.l3:

		mov	si, .init_str
	.loop3:
		cs lodsb
		test	al, al
		jz	.end
		pushf
;		call	far _int29_handler
		call	far reloc_call_int29_handler
		jmp	short .loop3
	.end:
		pop	si
		ret

.init_str	db	1bh, '[>1l'
	%ifndef INHERIT_CURSOR_POSITION
		db	1ah
	%endif
		db	0

		global	_init_nec98_getkey
_init_nec98_getkey:
		push	bx
		mov	ah, 5
		int	18h
		sub	bh, 1
		sbb	bx, bx
		or	ax, bx		; AX=FFFFh if no keystroke
		pop	bx
		ret

		global	_init_nec98_getshiftstate
_init_nec98_getshiftstate:
		mov	ah, 2
		int	18h
		mov	ah, 0
		ret

segment HMA_TEXT

; UBYTE ASMCFUNC get_crt_width(VOID)
		global	_get_crt_width
_get_crt_width:
		push	ds
		xor	ax, ax
		mov	ds, ax

		;xor	ah, ah
		mov	al, [CRT_STS_FLAG]
		test	al, 2
		jz	.w80
		mov	al, 40
		jmp	short .end
	.w80:
		mov	al, 80
	.end:
		pop	ds
		ret


; UBYTE ASMCFUNC get_crt_height(VOID)
		global	_get_crt_height
_get_crt_height:
		push	ds
		mov	ax, 60h
		mov	ds, ax

;		xor	ah, ah
		mov	al, [_scroll_bottom]
		inc	al

		pop	ds
		ret

;--------------------------------------

%ifndef USE_PUTCRT_SEG60

; VOID ASMCFUNC set_curpos(UBYTE x, UBYTE y)
		global	_set_curpos
_set_curpos:
		push	bp
		mov	bp, sp
		push	ds
		mov	ax, 60h
		mov	ds, ax
		mov	al, [bp + 4]
		mov	[_cursor_x], al
		mov	al, [bp + 6]
		mov	[_cursor_y], al
		call	update_curpos
		pop	ds
		pop	bp
		ret

; internal function (assume DS=60h)
update_curpos:
    mov al, 80
    mul byte [_cursor_y]
    add al, [_cursor_x]
    adc ah, 0
    add ax, ax
    mov dx, ax
    mov ah, 13h
    int 18h
    ret

; VOID ASMCFUNC update_cursor_view(VOID)
		global	_update_cursor_view
_update_cursor_view:
		push ds
		mov ax, 60h
		mov ds, ax
		mov ah, 12h
		cmp byte [_cursor_view], 0
		je .int18_and_exit
		call update_curpos
		mov ah, 11h
  .int18_and_exit:
		int 18h
		pop ds
		ret


; VOID ASMCFUNC put_crt(UBYTE x, UBYTE y, UWORD c)
		global	_put_crt
_put_crt:
		push	bp
		mov	bp, sp
		push	di
		push	ds
		push	es

		mov	ax, 0060h
		mov	ds, ax
		mov	es, [_text_vram_segment]
	%if 1
		mov	ax, 80		; todo 40cols mode
	%else
		call	_get_crt_width
	%endif
		mul	byte [bp + 6]	; ax = y * width
		mov	di, ax		; di = y * width
		xor	ah, ah
		mov	al, [bp + 4]	; ax = x
		add	di, ax		; di = y * width + x
		add	di, di		;     (y * width + x) * 2
		mov	ax, [bp + 8]	; char
		stosw
		xor	ah, ah
		mov	al, [_put_attr]
		add	di, 1ffeh
		stosw

		pop	es
		pop	ds
		pop	di
		pop	bp
		ret

; VOID ASMCFUNC put_crt_wattr(UBYTE x, UBYTE y, UWORD c, UBYTE a)
		global	_put_crt_wattr
_put_crt_wattr:
		push	bp
		mov	bp, sp
		push	di
		push	ds
		push	es

		mov	ax, 0060h
		mov	ds, ax
		mov	es, [_text_vram_segment]
	%if 1
		mov	ax, 80		; todo: 40cols mode
	%else
		call	_get_crt_width
	%endif
		mul	byte [bp + 6]	; y * width
		mov	di, ax
		xor	ah, ah
		mov	al, [bp + 4]	; x
		add	di, ax		; y * width + x
		add	di, di		; (y * width + x) * 2
		mov	ax, [bp + 8]	; char
		stosw

		mov	al, [_put_attr]
		or	al, [bp + 0ah]	; attr
		add	di, 1ffeh
		stosb

		pop	es
		pop	ds
		pop	di
		pop	bp
		ret

; VOID ASMCFUNC clear_crt(UBYTE x, UBYTE y)
		global	_clear_crt
_clear_crt:
		push	bp
		mov	bp, sp
		push	bx
		push	cx
		push	dx
		push	ds

		mov	dx, 60h
		mov	ds, dx
		;xor	dh, dh
		mov	dl, [_clear_char]
		mov	cl, [_clear_attr]

	%if 1
		mov	ax, 80		; todo: 40cols mode
	%else
		call	_get_crt_width
	%endif
		mov	ds, [_text_vram_segment]
		mul	byte [bp + 6]	; y * width
		xor	bh, bh
		mov	bl, [bp + 4]	; x
		add	bx, ax		; y * width + x
		shl	bx, 1		; (y * width + x) * 2
		mov	[bx], dx

		mov	[bx + 2000h], cl

		pop	ds
		pop	dx
		pop	cx
		pop	bx
		pop	bp
		ret

%endif ; !USE_PUTCRT_SEG60


; UBYTE FAR * ASMPASCAL nec98_programmable_key_table(unsigned index)
		global	NEC98_PROGRAMMABLE_KEY_TABLE
NEC98_PROGRAMMABLE_KEY_TABLE:
		pop	dx	; pop return address
		pop	ax	; index
		push	cs	; (return address seg)
		push	dx	; re-push return address
		jmp	FAR_CON_TGROUP: nec98_programmable_key_table_ax_far

; VOID ASMPASCAL nec98_set_cnvkey_table(UBYTE index)
		global NEC98_SET_CNVKEY_TABLE
NEC98_SET_CNVKEY_TABLE:
		pop	dx	; pop return address
		pop	ax	; index
		push	cs	; (return address seg)
		push	dx	; re-push return address
		jmp	FAR_CON_TGROUP: nec98_set_convkey_table_ax_far

; VOID ASMPASCAL nec98_get_programmable_key(void far *keydata, unsigned keyindex)
		global NEC98_GET_PROGRAMMABLE_KEY
NEC98_GET_PROGRAMMABLE_KEY:
		pop	ax
		push	cs
		push	ax
		jmp	FAR_CON_TGROUP: NEC98_GET_PROGRAMMABLE_KEY_FAR

; VOID ASMPASCAL nec98_set_programmable_key(const void far *keydata, unsigned keyindex)
		global NEC98_SET_PROGRAMMABLE_KEY
NEC98_SET_PROGRAMMABLE_KEY:
		pop	ax
		push	cs
		push	ax
		jmp	FAR_CON_TGROUP: NEC98_SET_PROGRAMMABLE_KEY_FAR

