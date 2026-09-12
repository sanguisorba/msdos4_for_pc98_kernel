; this is a part of FreeDOS(98) kernel
; included from kernel.asm

%ifndef INCLUDE_SUPSEG60

	[SECTION FAR_SUP_TEXTSEG]

		extern	NEC98_SUP_GET_SCSI_DEVICES_FAR
		extern	_nec98_sup_get_machine_type_far
		extern	NEC98_SUP_GET_DAUA_LIST_FAR

	; switch back to previous section (to be safe)
	__SECT__

%else	; INCLUDE_SUPSEG60


;--------------------------------------
; int DCh CL=09h AX=0000h Get type of SCSI devices
;
;VOID ASMSUPPASCAL_FAR nec98_sup_get_scsi_devices_far(VOID FAR *p);
		global	NEC98_SUP_GET_SCSI_DEVICES_FAR
NEC98_SUP_GET_SCSI_DEVICES_FAR:
		push	bp
		mov	bp, sp
arg_f {devlist,4}
		push	bx
		push	cx
		push	dx
		push	di
		push	ds
		push	es
		xor	ax, ax
		mov	ds, ax
		les	di, [.devlist]
		mov	dl, [0482h]	; scsi equips
		mov	dh, 1
		mov	bx, 0460h	; scsi device table 4bytes x 8 (ID0~7)
		mov	cx, 7
.lp:
		mov	al, 0
		test	dh, dl
		jnz	.write_dev	; assume HDD(=0) when equip flag is set
		mov	al, [bx]
		test	al, al
		jnz	.write_dev
		mov	al, 0ffh	; no device
.write_dev:
		stosb
		shl	dh, 1		; next ID
		add	bx, 4
		loop	.lp
		mov	al, 0ffh
		stosb			; ID7 = 0FFh (host)
		pop	es
		pop	ds
		pop	di
		pop	dx
		pop	cx
		pop	bx
		pop	bp
		retf	4


;--------------------------------------
; int DCh CL=12h Get MS-DOS product version and Machine Type
;
;UWORD ASMSUP_FAR nec98_sup_get_machine_type_far(VOID);
		global	_nec98_sup_get_machine_type_far
_nec98_sup_get_machine_type_far:
		push	dx
		push	ds
		xor	dx, dx
		mov	ds, dx
		mov	ax, [0500h]
		and	ax, 3801h
		test	byte [0458h], 80h	; pc-h98?
		jz	.chk_non_h98
;.h98:
		mov	dx, 1004h
		test	ax, 0800h		; h98 normal? (dx=1004/1005h)
		jz	.h98_l2
		mov	dx, 1101h		; h98 hi-reso (dx=1101/1102h)
.h98_l2:
		cmp	byte [0487h], 4;	; 486?
		jne	.exit_dx
		inc	dl			; h98 with i486 (dx=1005/1102h)
		jmp	short .exit_dx
.chk_non_h98:
		test	ax, 0800h
		jz	.chk_normal
;.chk_hires:
		mov	dx, 0101h
		test	byte [0481h], 40h
		jnz	.exit_dx
		cmp	ax, 0800h		; pc-98xa?
		jnz	.exit_dx
		mov	dl, 0
		jmp	short .exit_dx
.chk_normal:
		mov	dx, 4
		test	byte [0481h], 40h
		jnz	.exit_dx
		xor	dx, dx
		cmp	ax, 0000h		; pc-9801 original? (dx=0)
		je	.exit_dx
		inc	dx
		cmp	ax, 2000h		; pc-9801e/f/m? (dx=1)
		je	.exit_dx
		inc	dx
		cmp	ax, 3001h		; pc-9801u? (dx=2)
		je	.exit_dx
		inc	dx
		cmp	ax, 2001h		; pc-9801 common normal? (dx=3)
		je	.exit_dx
		inc	dx			; pc-98x1/pc98gs normal (dx=4)
.exit_dx:
		xchg	ax, dx
		pop	ds
		pop	dx
		retf


;--------------------------------------
; int DCh CL=13h Get DA/UA list
;
;VOID ASMSUPPASCAL_FAR nec98_sup_get_daua_list_far(VOID FAR *p);
		global	NEC98_SUP_GET_DAUA_LIST_FAR
NEC98_SUP_GET_DAUA_LIST_FAR:
		push	bp
		mov	bp, sp
arg_f {daualist,4}
		push	cx
		push	si
		push	di
		push	ds
		push	es
		mov	ax, PSP		; 0060h
		mov	ds, ax
		les	di, [.daualist]
		; +00h~+0Fh DA/UA A: to P:
		mov	si, 006ch
		mov	cx, 16
		rep movsb
		; +10h~19h 00
		xor	ax, ax
		mov	cl, 26 - 16
		rep stosb
		; +1Ah~4Dh flag+DA/UA x 26 (A: to Z:)
		mov	cl, 26
		mov	si, 2c86h
		rep movsw
		; 4Eh [0060:0038]
		mov	al, [0038h]
		stosb
		; 4Fh [0060:013B]
		mov	al, [013Bh]
		stosb
		; 50h [0060:0136] (_disk_last_access_unit)
		mov	al, [0136h]
		stosb
		; 51h~5Fh reserved
		mov	cl, 5fh - 51h + 1
		xor	ax, ax
		rep stosb
;.exit:
		pop	es
		pop	ds
		pop	di
		pop	si
		pop	cx
		pop	bp
		retf	4


%endif		; INCLUDE_SUPSEG60

