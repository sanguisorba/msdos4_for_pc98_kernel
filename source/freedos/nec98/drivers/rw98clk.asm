;
; File:
;                          rw98clk.asm
; Description:
;             read/write the PC-98 style clock from/to bios
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
; $Header:
;

	%include "../kernel/segs.inc"

segment	HMA_TEXT

;
; VOID ASMPASCAL ReadPC98Clock(CAL_DATA FAR *cal)
;
		global	READPC98CLOCK
READPC98CLOCK:
		sub	ah,ah
		jmp	short _common

;
; VOID ASMPASCAL WritePC98Clock(CAL_DATA FAR *cal)
;
		global	WRITEPC98CLOCK
WRITEPC98CLOCK:
		mov	ah,1

_common:
		push	bp
		mov	bp,sp
		push	es

		les	bx,[bp+4]
		int	1ch

		pop	es
		pop	bp
		ret	4

