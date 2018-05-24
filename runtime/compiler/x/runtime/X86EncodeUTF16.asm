; Copyright (c) 2014, 2017 IBM Corp. and others
;
; This program and the accompanying materials are made available under
; the terms of the Eclipse Public License 2.0 which accompanies this
; distribution and is available at https://www.eclipse.org/legal/epl-2.0/
; or the Apache License, Version 2.0 which accompanies this distribution and
; is available at https://www.apache.org/licenses/LICENSE-2.0.
;
; This Source Code may also be made available under the following
; Secondary Licenses when the conditions for such availability set
; forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
; General Public License, version 2 with the GNU Classpath
; Exception [1] and GNU General Public License, version 2 with the
; OpenJDK Assembly Exception [2].
;
; [1] https://www.gnu.org/software/classpath/license.html
; [2] http://openjdk.java.net/legal/assembly-exception.html
;
; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

                SURROGATE_MASK      equ 0f800h
                SURROGATE_MASK32    equ 0f800f800h
                SURROGATE_BITS      equ 0d800h
                SURROGATE_BITS32    equ 0d800d800h
                SSE_MIN_CHARS       equ 32

                include x/runtime/X86RegisterMap.inc

ifdef TR_HOST_64BIT
                _TEXT segment para 'CODE'
else
                assume cs:flat,ds:flat,ss:flat
                option NoOldMacros
                _TEXT segment para use32 'CODE'
endif

                public _encodeUTF16Big
                public _encodeUTF16Little

                align 16
_encodeUTF16Big_shufmask:
                dq 0607040502030001h
                dq 0e0f0c0d0a0b0809h

DefineUTF16EncodeHelper macro helperName:req, bigEndian:req
; UTF16 encoding for BMP characters
; pseudocode(uint8_t *dest, uint16_t *src, int n):
;    {
;    for (int i = 0; i < n; i++)
;       {
;       uint16_t c = src[i];
;       if ((c & SURROGATE_MASK) == SURROGATE_BITS) break;
; #if bigEndian
;       *dest++ = (uint8_t)(c >> 8);
;       *dest++ = (uint8_t)(c & 0xff);
; #else
;       *dest++ = (uint8_t)(c & 0xff);
;       *dest++ = (uint8_t)(c >> 8);
; #endif
;       }
;    return i;
;    }

; NB. c is a surrogate code unit
; iff SURROGATE_MIN = 0xd800 <= c <= 0xdfff = SURROGATE_MAX
; iff (c & SURROGATE_MASK) == SURROGATE_BITS,
; where SURROGATE_MASK = 0xf800, SURROGATE_BITS = 0xd800

; registers:
;    _rdi    dest ptr (into byte array)
;    _rsi    src  ptr (into char array)
;    _rdx    n
;    [_r]cx  c (one-at-a-time); tmp when using SSE
;    bx      c & SURROGATE_MASK (one-at-a-time)
;    _rax    original n / return value
;    xmm0    constant SURROGATE_MASK vector (0xf800..f800)
;    xmm1    constant SURROGATE_BITS vector (0xd800..d800)
;    xmm2    current 8 characters (8-at-a-time)
;    xmm3    surrogate bitmask
;    xmm4    byte shuffle mask (big-endian only)

                align 16
&helperName PROC
                ; Remember original count -
                ; will subtract at return to compute number converted
                mov _rax, _rdx
                cmp _rdx, 0
                je Lend_&helperName
                sub _rdi, _rsi  ; relative to _rsi, only advance _rsi
                cmp _rdx, SSE_MIN_CHARS
                jl Lresidue_loop_&helperName

Lprealign_&helperName:
                test _rsi, 0fh
                jz Laligned16_&helperName

                mov cx, word ptr [_rsi]

                ; return if surrogate
                mov bx, cx
                and bx, SURROGATE_MASK
                cmp bx, SURROGATE_BITS
                je Lend_&helperName

                ; not surrogate
if &bigEndian
                xchg cl, ch
endif
                mov word ptr [_rsi + _rdi], cx
                add _rsi, 2
                dec _rdx
                jg Lprealign_&helperName
                jmp Lend_&helperName

Laligned16_&helperName:
                sub _rdx, 8
                jl Lresidue_&helperName

                ; initialize constant vectors:
                ; SURROGATE_MASK
                mov ecx, SURROGATE_MASK32
                movd xmm0, ecx
                pshufd xmm0, xmm0, 0

                ; SURROGATE_BITS
                mov ecx, SURROGATE_BITS32
                movd xmm1, ecx
                pshufd xmm1, xmm1, 0

if &bigEndian
                ; shuffle mask for PSHUFB
                movdqa xmm4, oword ptr [_rip _encodeUTF16Big_shufmask]
endif

L8_at_a_time_&helperName:
                ; read 8 chars
                ; should this use movdqu, start once 8-byte aligned?
                movdqa xmm2, oword ptr [_rsi]

                ; jump to residue loop if any are surrogate
                movdqa xmm3, xmm2
                pand xmm3, xmm0
                pcmpeqw xmm3, xmm1
                ptest xmm3, xmm3    ; SSE4.1
                jnz Lresidue_&helperName

                ; no surrogates
if &bigEndian
                pshufb xmm2, xmm4   ; SSSE3
endif

                ; write 8 chars
                movdqu oword ptr [_rsi + _rdi], xmm2

                add _rsi, 16
                sub _rdx, 8
                jge L8_at_a_time_&helperName

Lresidue_&helperName:
                add _rdx, 8
                cmp _rdx, 0
                je Lend_&helperName

Lresidue_loop_&helperName:
                mov cx, word ptr [_rsi]

                ; return if surrogate
                mov bx, cx
                and bx, SURROGATE_MASK
                cmp bx, SURROGATE_BITS
                je Lend_&helperName

                ; not surrogate
if &bigEndian
                xchg cl, ch
endif
                mov word ptr [_rsi + _rdi], cx
                add _rsi, 2
                dec _rdx
                jg Lresidue_loop_&helperName

Lend_&helperName:
                sub _rax, _rdx
                ret

&helperName endp
endm

; Expand out the two helpers

                DefineUTF16EncodeHelper _encodeUTF16Big,    1
                DefineUTF16EncodeHelper _encodeUTF16Little, 0

_TEXT           ends

                end
