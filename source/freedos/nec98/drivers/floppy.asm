;
; File:
;                         floppy.asm
; Description:
;                   floppy disk driver primitives
;
;                       Copyright (c) 1995
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
; $Id: floppy.asm 980 2004-06-19 19:41:47Z perditionc $
;

%include "../kernel/segs.inc"
%include "../hdr/stacks.inc"
segment HMA_TEXT




%ifdef NEC98
; WORD ASMPASCAL fl_sense(WORD drive);
; modify [ax dx]
		global	FL_SENSE
FL_SENSE:
		pop	dx		; return address
		pop	ax		; DA/UA (AL only)
		push	dx		; restore address
		mov	ah, 04h
		int	1bh
		mov	al, ah		; ax = result
		sbb	ah, ah		;      FF??h if error
		and	al, ah		;      0000h if not error
		ret

; WORD ASMPASCAL fl_readid(WORD drive, UBYTE FAR *info)
; modify [ax bx cx dx]
		global	FL_READID
FL_READID:
		push	bp
		mov	bp, sp
arg drive,{info,4}
		mov	al, [.drive]
		mov	ah, 4ah
		xor	cx, cx
		xor	dx, dx
		int	1bh
		mov	al, ah
		jc	.exit
		push	ds
		lds	bx, [.info]
		mov	[bx], cx
		mov	[bx + 2], dx
		pop	ds
		mov	al, 0
	.exit:
		sbb	ah, ah
		pop	bp
		ret	6
%endif
;
; BOOL ASMPASCAL fl_reset(WORD drive);
;
; Reset both the diskette and hard disk system.
; returns TRUE if successful.
;

		global	FL_RESET
FL_RESET:
%ifdef NEC98
		pop	dx		; return address
		pop	ax		; DA/UA (AL only)
		push	dx		; restore address
  %if 1
		mov	ah, 07h		; Recalibrate (seek head to track 0) 
  %else
		mov	ah,03h		; BIOS reset diskette & fixed disk
  %endif
		int	1bh
%else
		pop	ax		; return address
		pop	dx		; drive (DL only)
		push	ax		; restore address
		mov	ah,0		; BIOS reset diskette & fixed disk
		int	13h
%endif
		sbb	ax,ax		; carry set indicates error, AX=-CF={-1,0}
		inc	ax		; ...return TRUE (1) on success,
		ret			; else FALSE (0) on failure

;
; COUNT ASMPASCAL fl_diskchanged(WORD drive);
;
; Read disk change line status.
; returns 1 if disk has changed, 0 if not, 0xFFFF if error.
;

		global	FL_DISKCHANGED
FL_DISKCHANGED:
%ifdef NEC98
		sub	ax,ax		; no change (NEC98HDD)
		ret	2

%else
		pop	ax		; return address
		pop	dx		; drive (DL only, 00h-7Fh)
		push	ax		; restore stack

		push	si		; preserve value
		mov	ah,16h	; read change status type
		xor	si,si		; RBIL: avoid crash on AT&T 6300
		int	13h
		pop	si		; restore

		sbb	al,al		; AL=-CF={-1,0} where 0==no change
		jnc   fl_dc		; carry set on error or disk change
		cmp	ah,6		; if AH==6 then disk change, else error
		jne	fl_dc		; if error, return -1
		mov	al, 1		; set change occurred
fl_dc:	cbw			; extend AL into AX, AX={1,0,-1}
		ret			; note: AH=0 on no change, AL set above
%endif

;
; Format tracks (sector should be 0).
; COUNT ASMPASCAL fl_format(WORD drive, WORD head, WORD track, WORD sector, WORD count, UBYTE FAR *buffer);
; Reads one or more sectors.
; COUNT ASMPASCAL fl_read  (WORD drive, WORD head, WORD track, WORD sector, WORD count, UBYTE FAR *buffer);
; Writes one or more sectors.
; COUNT ASMPASCAL fl_write (WORD drive, WORD head, WORD track, WORD sector, WORD count, UBYTE FAR *buffer);
; COUNT ASMPASCAL fl_verify(WORD drive, WORD head, WORD track, WORD sector, WORD count, UBYTE FAR *buffer);
;
; Returns 0 if successful, error code otherwise.
;

		global	FL_FORMAT
FL_FORMAT:
%ifdef NEC98
; modify [ax bx cx dx es] (fl_common)
		mov	ax,40h		; error (Equipment Check)
		ret	14
%else
                mov     ah,5            ; format track
                jmp     short fl_common
%endif

		global	FL_READ
FL_READ:
%ifdef NEC98
                mov     ah,6            ; read sector(s)
%else
                mov     ah,2            ; read sector(s)
%endif
                jmp short fl_common
                
		global	FL_VERIFY
FL_VERIFY:
%ifdef NEC98
                mov     ah,1            ; verify sector(s)
%else
                mov     ah,4            ; verify sector(s)
%endif
                jmp short fl_common
                
		global	FL_WRITE
FL_WRITE:
%ifdef NEC98
                mov     ah,5            ; write sector(s)
%else
                mov     ah,3            ; write sector(s)
%endif

fl_common:                
                push    bp              ; setup stack frame
                mov     bp,sp
arg drive, head, track, sector, count, {buffer,4}
%ifdef NEC98
		mov	al,[.drive]	; DA/UA
		push ax
		and al, 1ch	; FD? 
		cmp al, 10h
		pop ax
		jne .fl_common_hd
		or ah, 50h				; for FD: MFM, seek
	.fl_common_hd:
		mov	bx,[.count]	; count of sectors to read/write/...
		mov	cx,[.track]	; cylinder number
		mov	dh,[.head]	; head number
		mov	dl,[.sector]	; sector number
  %if 0
		dec	dl		; 0 base
  %endif
		les	bp,[.buffer]	; Load 32 bit buffer ptr into ES:BP

		int	1bh		; process sectors
%else

                mov     cx,[.track]     ; cylinder number

                mov     al,1            ; this should be an error code                     
                cmp     ch,3            ; this code can't write above 3FFh=1023
                ja      fl_error        ; as cylinder # is limited to 10 bits.

                xchg    ch,cl           ; ch=low 8 bits of cyl number
                ror     cl,1            ; bits 8-9 of cylinder number...
                ror     cl,1            ; ...to bits 6-7 in CL
                or      cl,[.sector]	; or in the sector number (bits 0-5)

                mov     al,[.count]     ; count of sectors to read/write/...
                les     bx,[.buffer]    ; Load 32 bit buffer ptr into ES:BX

                mov     dl,[.drive]     ; drive (if or'ed 80h its a hard drive)
                mov     dh,[.head]      ; get the head number

                int     13h             ; process sectors
%endif
		sbb	al,al		; carry: al=ff, else al=0
		and	al,ah		; carry: error code, else 0
fl_error:
                mov     ah,0            ; extend AL into AX without sign extension
                pop     bp
                ret     14

;
; COUNT ASMPASCAL fl_lba_ReadWrite(BYTE drive, WORD mode, void FAR * dap_p);
;
; Returns 0 if successful, error code otherwise.
;

		global  FL_LBA_READWRITE
FL_LBA_READWRITE:
%ifdef NEC98
		mov	ax,40h		; error (Equipment Check)
		ret     8
%else
		push    bp              ; setup stack frame
		mov     bp,sp
		
		push    ds
		push    si              ; wasn't in kernel < KE2024Bo6!!

arg drive, mode, {dap_p,4}
		mov     dl,[.drive]     ; drive (if or'ed with 80h a hard drive)
		mov     ax,[.mode]      ; get the command
		lds     si,[.dap_p]     ; get far dap pointer
		int     13h             ; read from/write to drive
		
		pop     si
		pop     ds

		pop     bp

		mov     al,ah           ; place any error code into al
		mov     ah,0            ; zero out ah           
		ret     8
%endif

;
; COUNT ASMPASCAL fl_lba_readwrite_nec98(BYTE drive, WORD mode, ULONG lba_address, WORD count_by_byte, UBYTE FAR *buffer);
;
%ifdef NEC98
		global	FL_LBA_READWRITE_NEC98
FL_LBA_READWRITE_NEC98:
		push	bp
		mov	bp, sp
arg drive, mode, {lba_address,4}, count_by_byte, {buffer,4}
		mov	al, [.drive]
		and	al, 7fh
		mov	dl, al
		and	dl, 0f0h
		jz	.hd		; sasi/ide (DAUA=0x)
		cmp	dl, 20h		; scsi     (DAUA=2x)
		jz	.hd
.no_hd:
		mov	ax, 40h		; error (Equipment check)
		jmp	short .exit
.hd:
		push	bx
		push	cx
		push	es
		mov	ah, [.mode + 1]
		mov	bx, [.count_by_byte]
		mov	cx, [.lba_address]
		mov	dx, [.lba_address + 2]
		les	bp, [.buffer]
		int	1bh
		xchg	al, ah
		sbb	bx, bx
		and	ax, bx
		pop	es
		pop	cx
		pop	bx
.exit:
		pop	bp
		ret	2 + 2 + 4 + 2 + 4
%endif

;
; void ASMPASCAL fl_readkey (void);
;

		global	FL_READKEY
%ifdef NEC98
FL_READKEY:
		xor	ah,ah
		int	18h
		ret
%else
FL_READKEY:     xor	ah, ah
		int	16h
		ret
%endif

;
; COUNT ASMPASCAL fl_setdisktype (WORD drive, WORD type);
;

		global	FL_SETDISKTYPE
FL_SETDISKTYPE:
%ifdef NEC98
		mov	ax,40h		; error (Equipment Check)
		ret	4
%else
		pop	bx		; return address
		pop	ax		; disk format type (al)
		pop	dx		; drive number (dl)
		push	bx		; restore stack
		mov	ah,17h	; floppy set disk type for format
		int	13h
ret_AH:
		mov     al,ah           ; place any error code into al
		mov     ah,0            ; zero out ah           
		ret
%endif
                        
;
; COUNT ASMPASCAL fl_setmediatype (WORD drive, WORD tracks, WORD sectors);
;
		global	FL_SETMEDIATYPE
FL_SETMEDIATYPE:
%ifdef NEC98
		mov	ax,40h		; error (Equipment Check)
		ret	6
%else
		pop	ax		; return address
		pop	bx		; sectors/track
		pop	cx		; number of tracks
		pop	dx		; drive number
		push	ax		; restore stack
		push	di

		dec	cx		; number of cylinders - 1 (last cyl number)
		xchg	ch,cl		; CH=low 8 bits of last cyl number
               
		ror	cl,1		; extract bits 8-9 of cylinder number...
		ror	cl,1		; ...into cl bit 6-7
                
		or	cl,bl		; sectors/track (bits 0-5) or'd with high cyl bits 7-6

		mov	ah,18h	; disk set media type for format
		int	13h
		jc	skipint1e

		push	es
                xor     dx,dx
                mov     es,dx
		cli
                pop     word [es:0x1e*4+2] ; set int 0x1e table to es:di
                mov     [es:0x1e*4  ], di
		sti
skipint1e:		
                pop     di
		jmp	short ret_AH
                
%endif
