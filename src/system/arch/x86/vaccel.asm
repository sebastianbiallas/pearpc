;
;	PearPC
;	vaccel.asm
;
;	Copyright (C) 2004 Sebastian Biallas (sb@biallas.net)
;
;	This program is free software; you can redistribute it and/or modify
;	it under the terms of the GNU General Public License version 2 as
;	published by the Free Software Foundation.
;
;	This program is distributed in the hope that it will be useful,
;	but WITHOUT ANY WARRANTY; without even the implied warranty of
;	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;	GNU General Public License for more details.
;
;	You should have received a copy of the GNU General Public License
;	along with this program; if not, write to the Free Software
;	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;

extern gX86RGBMask
global x86_convert_2be555_to_2le555
global x86_convert_2be555_to_2le565
global x86_convert_2be555_to_4le888
global x86_convert_4be888_to_4le888

d1:	dd	0x00ff00ff
	dd	0x00ff00ff
d2:	dd	0xff00ff00
	dd	0xff00ff00

_2be555_mask_r	dd	0x7c007c00
		dd	0x7c007c00
_2be555_mask_g	dd	0x03e003e0
		dd	0x03e003e0
_2be555_mask_b	dd	0x001f001f
		dd	0x001f001f

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax -- number of pixels to convert
;;	    edx -- input
;;          ecx -- output

x86_convert_2be555_to_2le555:
	shr		eax, 3		; we can convert 8 pixels at a time
.loop:
	movq		mm1, [edx]
	movq		mm3, [edx+8]

	;; convert big to little endian
	movq		mm2, mm1
	movq		mm4, mm3
	pand		mm1, [d1]
	pand		mm2, [d2]
	pand		mm3, [d1]
	pand		mm4, [d2]
	psllw		mm1, 8
	psrlw		mm2, 8
	psllw		mm3, 8
	psrlw		mm4, 8
	por		mm1, mm2
	por		mm3, mm4

	movq		[ecx], mm1
	movq		[ecx+8], mm3
	add		edx, 16
	add		ecx, 16
	dec		eax
	jnz		.loop
	emms
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax -- number of pixels to convert
;;	    edx -- input
;;          ecx -- output

x86_convert_2be555_to_2le565:
	shr		eax, 3		; we can convert 8 pixels at a time
.loop:
	movq		mm1, [edx]
	movq		mm3, [edx+8]
	;; convert big to little endian
	movq		mm2, mm1
	movq		mm4, mm3
	pand		mm1, [d1]
	pand		mm2, [d2]
	pand		mm3, [d1]
	pand		mm4, [d2]
	psllw		mm1, 8
	psrlw		mm2, 8
	psllw		mm3, 8
	psrlw		mm4, 8
	por		mm1, mm2
	por		mm4, mm3

	movq		mm2, mm1
	movq		mm3, mm1
	movq		mm5, mm4
	movq		mm6, mm4
	pand		mm1, [_2be555_mask_r]
	pand		mm2, [_2be555_mask_g]
	pand		mm3, [_2be555_mask_b]
	pand		mm4, [_2be555_mask_r]
	pand		mm5, [_2be555_mask_g]
	pand		mm6, [_2be555_mask_b]
	psllw		mm1, 1		; red
	psllw		mm2, 1		; green
;	psllw		mm3, 0		; blue
	psllw		mm4, 1		; red
	psllw		mm5, 1		; green
;	psllw		mm6, 0		; blue
	por		mm1, mm2
	por		mm4, mm5
	por		mm1, mm3
	por		mm4, mm6
	movq		[ecx], mm1
	movq		[ecx+8], mm4
	add		edx, 16
	add		ecx, 16
	dec		eax
	jnz		.loop
	emms
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax -- number of pixels to convert
;;	    edx -- input
;;          ecx -- output

x86_convert_2be555_to_4le888:
	shr		eax, 2		; we can convert 4 pixels at a time
	pxor		mm0, mm0
.loop:
	movq		mm1, [edx]

	;; convert big to little endian
	movq		mm3, mm1
	pand		mm1, [d1]
	pand		mm3, [d2]
	psllw		mm1, 8
	psrlw		mm3, 8
	por		mm1, mm3

	movq		mm2, mm1
	movq		mm3, mm1
	pand		mm1, [_2be555_mask_r]
	pand		mm2, [_2be555_mask_g]
	pand		mm3, [_2be555_mask_b]
	movq		mm4, mm1
	movq		mm5, mm2
	movq		mm6, mm3
	punpcklwd	mm1, mm0
	punpcklwd	mm2, mm0
	punpcklwd	mm3, mm0
	punpckhwd	mm4, mm0
	punpckhwd	mm5, mm0
	punpckhwd	mm6, mm0
	pslld		mm1, 16-10+3	; red
	pslld		mm2, 8-5+3	; green
	pslld		mm3, 0+3	; blue
	pslld		mm4, 16-10+3	; red
	pslld		mm5, 8-5+3	; green
	pslld		mm6, 0+3	; blue
	por		mm1, mm2
	por		mm1, mm3
	por		mm4, mm5
	por		mm4, mm6
	movq		[ecx], mm1
	movq		[ecx+8], mm4
	add		edx, 8
	add		ecx, 16
	dec		eax
	jnz		.loop
	emms
	ret


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;	IN: eax -- number of pixels to convert
;;	    edx -- input
;;          ecx -- output

x86_convert_4be888_to_4le888:
	push		ebx
	push		ebp
	push		esi
	push		edi
	shr		eax, 2		; we can convert 4 pixels at a time
.loop1
	mov		ebx, [edx]
	mov		ebp, [edx+4]
	mov		esi, [edx+8]
	mov		edi, [edx+12]
	;; convert big to little endian
	bswap		ebx
	bswap		ebp
	bswap		esi
	bswap		edi
	mov		[ecx], ebx
	mov		[ecx+4], ebp
	mov		[ecx+8], esi
	mov		[ecx+12], edi
	add		edx, 16
	add		ecx, 16
	dec		eax
	jnz		.loop1

	pop		edi
	pop		esi
	pop		ebp
	pop		ebx
	ret
