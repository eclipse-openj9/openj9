; Copyright (c) 2000, 2017 IBM Corp. and others
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
; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

USE_SSE3 equ                 0  ;; Protects the SSE3 prototype, requires an SSE3 processor.

ALIGN_THRESHOLD equ         24  ;; Align copies greater than or equal to this size
LONG_COPY_THRESHOLD equ     16  ;; Use long copy if greater than or equal to this size
MOV_THRESHOLD equ           64  ;; Use movq or movdqu if greater than or equal than this size
FWD_SSE_THRESHOLD equ       32  ;; Attempt SSE copy if greater than or equal to this size

; DEBUGSTATS equ               1  ;; Output collected statistics to the jit log file.
; RECORD_ARRAYCOPY_INFO equ    1  ;; Call arraycopy details collector function. This is intrusive.

                .486p
                .xmm
                assume cs:flat,ds:flat,ss:flat
                _DATA SEGMENT PARA USE32 PUBLIC 'DATA'

                public multiprocessor
                align   04h
multiprocessor:
                db 0
          _DATA ends

                ; SSE forward byte array copy alignment table
JumpTableStart fsacAlignTable
                dd      offset FLAT: fsacAligned
                dd      offset FLAT: fsacPad1
                dd      offset FLAT: fsacPad2
                dd      offset FLAT: fsacPad3
                dd      offset FLAT: fsacPad4
                dd      offset FLAT: fsacPad5
                dd      offset FLAT: fsacPad6
                dd      offset FLAT: fsacPad7
                dd      offset FLAT: fsacPad8
                dd      offset FLAT: fsacPad9
                dd      offset FLAT: fsacPad10
                dd      offset FLAT: fsacPad11
                dd      offset FLAT: fsacPad12
                dd      offset FLAT: fsacPad13
                dd      offset FLAT: fsacPad14
                dd      offset FLAT: fsacPad15
JumpTableEnd fsacAlignTable

                ; Backward direction array copy table
JumpTableStart bwdCopyTable
                dd      offset FLAT: bwdCopy0
                dd      offset FLAT: bwdCopy1
                dd      offset FLAT: bwdCopy2
                dd      offset FLAT: bwdCopy3
                dd      offset FLAT: bwdCopy4
                dd      offset FLAT: bwdCopy5
                dd      offset FLAT: bwdCopy6
                dd      offset FLAT: bwdCopy7
                dd      offset FLAT: bwdCopy8
                dd      offset FLAT: bwdCopy9
                dd      offset FLAT: bwdCopy10
                dd      offset FLAT: bwdCopy11
                dd      offset FLAT: bwdCopy12
                dd      offset FLAT: bwdCopy13
                dd      offset FLAT: bwdCopy14
                dd      offset FLAT: bwdCopy15
                dd      offset FLAT: bwdCopy16
                dd      offset FLAT: bwdCopy17
                dd      offset FLAT: bwdCopy18
                dd      offset FLAT: bwdCopy19
                dd      offset FLAT: bwdCopy20
                dd      offset FLAT: bwdCopy21
                dd      offset FLAT: bwdCopy22
                dd      offset FLAT: bwdCopy23
                dd      offset FLAT: bwdCopy24
                dd      offset FLAT: bwdCopy25
                dd      offset FLAT: bwdCopy26
                dd      offset FLAT: bwdCopy27
                dd      offset FLAT: bwdCopy28
                dd      offset FLAT: bwdCopy29
                dd      offset FLAT: bwdCopy30
                dd      offset FLAT: bwdCopy31
                dd      offset FLAT: bwdCopy32
JumpTableEnd bwdCopyTable

                ; forward direction array copy table
JumpTableStart fwdCopyTable
                dd      offset FLAT: fwdCopy0
                dd      offset FLAT: fwdCopy1
                dd      offset FLAT: fwdCopy2
                dd      offset FLAT: fwdCopy3
                dd      offset FLAT: fwdCopy4
                dd      offset FLAT: fwdCopy5
                dd      offset FLAT: fwdCopy6
                dd      offset FLAT: fwdCopy7
                dd      offset FLAT: fwdCopy8
                dd      offset FLAT: fwdCopy9
                dd      offset FLAT: fwdCopy10
                dd      offset FLAT: fwdCopy11
                dd      offset FLAT: fwdCopy12
                dd      offset FLAT: fwdCopy13
                dd      offset FLAT: fwdCopy14
                dd      offset FLAT: fwdCopy15
                dd      offset FLAT: fwdCopy16
                dd      offset FLAT: fwdCopy17
                dd      offset FLAT: fwdCopy18
                dd      offset FLAT: fwdCopy19
                dd      offset FLAT: fwdCopy20
                dd      offset FLAT: fwdCopy21
                dd      offset FLAT: fwdCopy22
                dd      offset FLAT: fwdCopy23
                dd      offset FLAT: fwdCopy24
                dd      offset FLAT: fwdCopy25
                dd      offset FLAT: fwdCopy26
                dd      offset FLAT: fwdCopy27
                dd      offset FLAT: fwdCopy28
                dd      offset FLAT: fwdCopy29
                dd      offset FLAT: fwdCopy30
                dd      offset FLAT: fwdCopy31
                dd      offset FLAT: fwdCopy32
JumpTableEnd fwdCopyTable

                ; forward direction halfword arraycopy table
JumpTableStart fwdHalfWordCopyTable
                dd      offset FLAT: fwdCopy0
                dd      0
                dd      offset FLAT: SSEfwdCopy2
                dd      0
                dd      offset FLAT: SSEfwdCopy4
                dd      0
                dd      offset FLAT: SSEfwdCopy6
                dd      0
                dd      offset FLAT: SSEfwdCopy8
                dd      0
                dd      offset FLAT: SSEfwdCopy10
                dd      0
                dd      offset FLAT: SSEfwdCopy12
                dd      0
                dd      offset FLAT: SSEfwdCopy14
                dd      0
                dd      offset FLAT: SSEfwdCopy16
                dd      0
                dd      offset FLAT: SSEfwdCopy18
                dd      0
                dd      offset FLAT: SSEfwdCopy20
                dd      0
                dd      offset FLAT: SSEfwdCopy22
                dd      0
                dd      offset FLAT: SSEfwdCopy24
                dd      0
                dd      offset FLAT: SSEfwdCopy26
                dd      0
                dd      offset FLAT: SSEfwdCopy28
                dd      0
                dd      offset FLAT: SSEfwdCopy30
                dd      0
                dd      offset FLAT: SSEfwdCopy32
                dd      0
                dd      offset FLAT: SSEfwdCopy34
                dd      0
                dd      offset FLAT: SSEfwdCopy36
                dd      0
                dd      offset FLAT: SSEfwdCopy38
                dd      0
                dd      offset FLAT: SSEfwdCopy40
                dd      0
                dd      offset FLAT: SSEfwdCopy42
                dd      0
                dd      offset FLAT: SSEfwdCopy44
                dd      0
                dd      offset FLAT: SSEfwdCopy46
                dd      0
                dd      offset FLAT: SSEfwdCopy48
JumpTableEnd fwdHalfWordCopyTable

_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

                public  _arrayCopy
                public  _wordArrayCopy
                public  _halfWordArrayCopy
                public  _forwardWordArrayCopy
                public  _forwardHalfWordArrayCopy
                public  _forwardArrayCopy
                public  _backwardWordArrayCopy
                public  _backwardHalfWordArrayCopy
                public  _backwardArrayCopy
                public  _arrayCopyAggressive
                public  _wordArrayCopyAggressive
                public  _halfWordArrayCopyAggressive

                public  fwdHalfWordCopyTable
                public  _SSEforwardHalfWordArrayCopy
                public  _SSEforwardArrayCopy
                public  _SSEforwardArrayCopyAMDOpteron

                public  _forwardSSEArrayCopy
                public  _forwardSSEArrayCopyNoAlignCheck

                public  _shortArrayCopy         ; Not used, this helper entry can be reused for something else.
                public  _forwardArrayCopy2      ; Not used, this helper entry can be reused for something else.
                public  _SSEforwardArrayCopyAggressive

                align   16
;
; A c-style memmove with no assumptions on the element size
; or copy direction required.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_arrayCopy PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardArrayCopy
                jmp     LforwardArrayCopy

                ; --------------------------------------------------------
                ; NOTE:
                ; These copy sequences are shared by all forward copy
                ; procedures. They are placed here because they are nearest
                ; to their invokers. Odd size copies are only used for byte
                ; copy procedures.
                ; --------------------------------------------------------
                ; Forward copy table section for length modulus 4 = 3
                ; --------------------------------------------------------
fwdCopy31:      mov     ecx, dword ptr [esi-31]
                mov     dword ptr [edi-31], ecx
fwdCopy27:      mov     ecx, dword ptr [esi-27]
                mov     dword ptr [edi-27], ecx
fwdCopy23:      mov     ecx, dword ptr [esi-23]
                mov     dword ptr [edi-23], ecx
fwdCopy19:      mov     ecx, dword ptr [esi-19]
                mov     dword ptr [edi-19], ecx
fwdCopy15:      mov     ecx, dword ptr [esi-15]
                mov     dword ptr [edi-15], ecx
fwdCopy11:      mov     ecx, dword ptr [esi-11]
                mov     dword ptr [edi-11], ecx
fwdCopy7:       mov     ecx, dword ptr [esi-7]
                mov     dword ptr [edi-7], ecx
fwdCopy3:       movzx   ecx, word ptr [esi-3]
                mov     word ptr [edi-3], cx
                movzx   ecx, byte ptr [esi-1]
                mov     byte ptr [edi-1], cl
                ret

                ; --------------------------------------------------------
                ; Forward copy table section for length modulus 4 =
                ; --------------------------------------------------------
fwdCopy29:      mov     ecx, dword ptr [esi-29]
                mov     dword ptr [edi-29], ecx
fwdCopy25:      mov     ecx, dword ptr [esi-25]
                mov     dword ptr [edi-25], ecx
fwdCopy21:      mov     ecx, dword ptr [esi-21]
                mov     dword ptr [edi-21], ecx
fwdCopy17:      mov     ecx, dword ptr [esi-17]
                mov     dword ptr [edi-17], ecx
fwdCopy13:      mov     ecx, dword ptr [esi-13]
                mov     dword ptr [edi-13], ecx
fwdCopy9:       mov     ecx, dword ptr [esi-9]
                mov     dword ptr [edi-9], ecx
fwdCopy5:       mov     ecx, dword ptr [esi-5]
                mov     dword ptr [edi-5], ecx
fwdCopy1:       movzx   ecx, byte ptr [esi-1]
                mov     byte ptr [edi-1], cl
                ret
_arrayCopy endp
;

fwdHalfWordCopy PROC NEAR

SSEfwdCopy48:   movq     xmm7, qword ptr [esi-48]
                movq     qword ptr [edi-48], xmm7
SSEfwdCopy40:   movq     xmm7, qword ptr [esi-40]
                movq     qword ptr [edi-40], xmm7
SSEfwdCopy32:   movq     xmm7, qword ptr [esi-32]
                movq     qword ptr [edi-32], xmm7
SSEfwdCopy24:   movq     xmm7, qword ptr [esi-24]
                movq     qword ptr [edi-24], xmm7
SSEfwdCopy16:   movq     xmm7, qword ptr [esi-16]
                movq     qword ptr [edi-16], xmm7
SSEfwdCopy8:    movq     xmm7, qword ptr [esi-8]
                movq     qword ptr [edi-8], xmm7
                ret

SSEfwdCopy46:   movq     xmm7, qword ptr [esi-46]
                movq     qword ptr [edi-46], xmm7
SSEfwdCopy38:   movq     xmm7, qword ptr [esi-38]
                movq     qword ptr [edi-38], xmm7
SSEfwdCopy30:   movq     xmm7, qword ptr [esi-30]
                movq     qword ptr [edi-30], xmm7
SSEfwdCopy22:   movq     xmm7, qword ptr [esi-22]
                movq     qword ptr [edi-22], xmm7
SSEfwdCopy14:   movq     xmm7, qword ptr [esi-14]
                movq     qword ptr [edi-14], xmm7
SSEfwdCopy6:    mov      ecx, dword ptr [esi-6]
                mov      dword ptr [edi-6], ecx
                mov      cx, word ptr [esi-2]
                mov      word ptr [edi-2], cx
                ret

SSEfwdCopy44:   movq     xmm7, qword ptr [esi-44]
                movq     qword ptr [edi-44], xmm7
SSEfwdCopy36:   movq     xmm7, qword ptr [esi-36]
                movq     qword ptr [edi-36], xmm7
SSEfwdCopy28:   movq     xmm7, qword ptr [esi-28]
                movq     qword ptr [edi-28], xmm7
SSEfwdCopy20:   movq     xmm7, qword ptr [esi-20]
                movq     qword ptr [edi-20], xmm7
SSEfwdCopy12:   movq     xmm7, qword ptr [esi-12]
                movq     qword ptr [edi-12], xmm7
SSEfwdCopy4:    mov      ecx, dword ptr [esi-4]
                mov      dword ptr [edi-4], ecx
                ret

SSEfwdCopy42:   movq     xmm7, qword ptr [esi-42]
                movq     qword ptr [edi-42], xmm7
SSEfwdCopy34:   movq     xmm7, qword ptr [esi-34]
                movq     qword ptr [edi-34], xmm7
SSEfwdCopy26:   movq     xmm7, qword ptr [esi-26]
                movq     qword ptr [edi-26], xmm7
SSEfwdCopy18:   movq     xmm7, qword ptr [esi-18]
                movq     qword ptr [edi-18], xmm7
SSEfwdCopy10:   movq     xmm7, qword ptr [esi-10]
                movq     qword ptr [edi-10], xmm7
SSEfwdCopy2:    mov      cx, word ptr [esi-2]
                mov      word ptr [edi-2], cx
                ret

fwdHalfWordCopy ENDP

               align    16
;
; A c-style memcpy based on SSE with no assumptions on the element size
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
;

_SSEforwardArrayCopyAggressive PROC NEAR
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
LSSEfwdCopyAggressiveStart:

LSSEfwdCopyAggressive:
                ; make some temp registers
                push    eax
                
                ; use single index register to avoid one extra add in loops
                sub     edi, esi
                
                ; for small copies just go down to copy, 1, 2 and 4 bytes
                cmp     ecx, 8
                jb      LSSEfwdCopyLessThanEight
                
                
                ; align the source to 8 bytes
                ; we rely on the compare at the top to handle lengths smaller than 8 bytes
                ; we can safely copy at least 24 bytes
                
                test    esi, 1
                jz      LSSEfwdCopySourceAligned2
                mov     al, byte ptr [esi]
                mov     byte ptr [edi + esi], al
                add     esi, 1
                sub     ecx, 1
              
LSSEfwdCopySourceAligned2:                
                test    esi, 2
                jz      LSSEfwdCopySourceAligned4
                mov     ax, word ptr [esi]
                mov     word ptr [edi + esi], ax
                add     esi, 2
                sub     ecx, 2

LSSEfwdCopySourceAligned4:                
                test    esi, 4
                jz      LSSEfwdCopySourceAligned
                mov     eax, dword ptr [esi]
                mov     dword ptr [edi + esi], eax
                add     esi, 4
                sub     ecx, 4

LSSEfwdCopySourceAligned:
                ; make x 8 counter in eax
                ; we want to move at least chunks of 8
                mov     eax, ecx
                shr     eax, 3
                ; see if you can do at least one step of the unroll
                sub     eax, 8
                jge     LSSEfwdCopyUnrolled
                ; if not add back the counter and go 8 bytes at a time
                add     eax, 8
                jnz     LSSEfwdCopyEightAtATime
                ; we should never reach here since the top check handles
                ; lengths < 24, but anyway it is better to have it
                jmp     LSSEfwdCopyLessThanEight                
                
                align   16
LSSEfwdCopyUnrolled:                
                ; do unrolled copy of 64 bytes at a time
                movq    xmm0, qword ptr [esi]
                movq    xmm1, qword ptr [esi + 8]
                movq    xmm2, qword ptr [esi + 16]
                movq    qword ptr [edi + esi], xmm0
                movq    xmm3, qword ptr [esi + 24]
                movq    qword ptr [edi + esi + 8], xmm1
                movq    xmm0, qword ptr [esi + 32]
                movq    qword ptr [edi + esi + 16], xmm2
                movq    xmm1, qword ptr [esi + 40]
                movq    qword ptr [edi + esi + 24], xmm3
                movq    xmm2, qword ptr [esi + 48]
                movq    qword ptr [edi + esi + 32], xmm0
                movq    xmm3, qword ptr [esi + 56]
                movq    qword ptr [edi + esi + 40], xmm1
                movq    qword ptr [edi + esi + 48], xmm2
                movq    qword ptr [edi + esi + 56], xmm3
                
                add     esi, 64                
LSSEfwdCopyCheckForUnroll:                
                sub     eax, 8
                jge     LSSEfwdCopyUnrolled
                add     eax, 8
                jz      LSSEfwdCopyLessThanEight

                align   16
LSSEfwdCopyEightAtATime:
                movq    xmm0, qword ptr [esi]
                movq    qword ptr [edi + esi], xmm0
                add     esi, 8
                dec     eax
                jg      LSSEfwdCopyEightAtATime                  
               
LSSEfwdCopyLessThanEight:
                ; now we check the original counter for residue
                ; below 8 bytes
                
                test    ecx, 4
                jz      LSSEfwdCopyResidue2
                mov     eax, dword ptr [esi]
                mov     dword ptr [edi + esi], eax
                add     esi, 4

LSSEfwdCopyResidue2:
                test    ecx, 2
                jz      LSSEfwdCopyResidue1
                mov     ax, word ptr [esi]
                mov     word ptr [edi + esi], ax
                add     esi, 2
         
LSSEfwdCopyResidue1:
                test    ecx, 1
                jz      LSSEfwdCopyEnd
                mov     al, byte ptr [esi]
                mov     byte ptr [edi + esi], al                                
LSSEfwdCopyEnd:        
                ; restore temp registers
                pop     eax
                
                ret
_SSEforwardArrayCopyAggressive endp

               align    16
;
; A c-style memcpy based on SSE with no assumptions on the element size
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
;
_SSEforwardArrayCopy PROC NEAR
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     short LSSEfwdCopyLong   ; Use unsigned compare

                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

LSSEfwdCopyLong:                           ; Entry when target is byte aligned
                test    edi, 3          ; Perform required alignemnt
                jz      short SSEfwdCopyLongAligned
                test    edi, 1
                jz      short SSEfclAlign2
                sub     ecx, 1
                movsb
LSSEfwdHalfWordCopyLong:                   ; Entry when target is two byte aligned
                test    edi, 2
                jz      short SSEfwdCopyLongAligned
SSEfclAlign2:
                sub     ecx, 2
                movsw
SSEfwdCopyLongAligned:
                cmp     ecx, MOV_THRESHOLD
                jae     short fclMovQ   ; Use unsigned compare
SSEfwdOverlapped:                
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                test    ecx, 7
                jz      short SSEfclLoopNoResidue
SSEfclLoop:
                movq    xmm7, qword ptr [esi+ecx]
                movq    qword ptr [edi+ecx], xmm7
                add     ecx, 8
                cmp     ecx, -8
                jbe     short SSEfclLoop   ; Use unsigned compare
                neg     ecx
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                align   16
SSEfclLoopNoResidue:
                movq    xmm7, qword ptr [esi+ecx]
                movq    qword ptr [edi+ecx], xmm7
                add     ecx, 8
                jl      short SSEfclLoopNoResidue
                ret

				; you can not call this function if your arraycopy length is less than 32
                align   16
fclMovQ:	
				push eax
				mov eax, esi
				sub eax, edi
				and eax, 31
				jz fclNoOverlap
				pop eax
				jmp SSEfwdOverlapped
fclNoOverlap:				
				mov eax, ecx
				sub ecx, edi
				sub ecx, eax
				and ecx, 31
				sub eax, ecx
				jle fclMovQEndBytes
				add edi, ecx
				add esi, ecx
				neg ecx
				movq mm0, qword ptr [esi+ecx]
				movq mm1, qword ptr [esi+ecx+8]
				movq mm2, qword ptr [esi+ecx+16]
				movq mm3, qword ptr [esi+ecx+24]
				movq qword ptr [edi+ecx], mm0
				movq qword ptr [edi+ecx+8], mm1
				movq qword ptr [edi+ecx+16], mm2
				movq qword ptr [edi+ecx+24], mm3
				mov ecx, eax
				and eax, 31
				shr ecx, 5
				jz fclMovQEndBytes
				sub edi, esi
fclMovQLoop:		
				movq mm0, qword ptr [esi]
				movq mm1, qword ptr [esi+8]
				movq mm2, qword ptr [esi+16]
				movq mm3, qword ptr [esi+24]
				movq qword ptr [edi+esi], mm0
				movq qword ptr [edi+esi+8], mm1
				movq qword ptr [edi+esi+16], mm2
				movq qword ptr [edi+esi+24], mm3
				add esi, 32
				dec ecx
				jnz fclMovQLoop
				add edi, esi
fclMovQEndBytes:  
				add ecx, eax
				sub ecx, 32
				movq mm0, qword ptr [esi+ecx]
				movq mm1, qword ptr [esi+ecx+8]
				movq mm2, qword ptr [esi+ecx+16]
				movq mm3, qword ptr [esi+ecx+24]
				movq qword ptr [edi+ecx], mm0
				movq qword ptr [edi+ecx+8], mm1
				movq qword ptr [edi+ecx+16], mm2
				movq qword ptr [edi+ecx+24], mm3
				emms
				pop eax
                ret
_SSEforwardArrayCopy endp

                align   16
; ArrayCopy optimized for the Opteron processor
; A c-style memcpy based on SSE with no assumptions on the element size
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
;
_SSEforwardArrayCopyAMDOpteron PROC NEAR
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     short LSSEfwdCopyLongAMDOpteron   ; Use unsigned compare

                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable      ; Jump table will do a ret

LSSEfwdCopyLongAMDOpteron:                           ; Entry when target is byte align	
                test    edi, 3          ; Perform required alignemnt
                jz      short SSEfwdCopyLongAlignedAMDOpteron
                test    edi, 1
                jz      short SSEfclAlign2AMDOpteron
                sub     ecx, 1
                movsb
LSSEfwdHalfWordCopyLongAMDOpteron:                   ; Entry when target is two byte aligned
                test    edi, 2
                jz      short SSEfwdCopyLongAlignedAMDOpteron
SSEfclAlign2AMDOpteron:
                sub     ecx, 2
                movsw
SSEfwdCopyLongAlignedAMDOpteron:
                cmp     ecx, MOV_THRESHOLD
                jae     short fclMovDQUAMDOpteron   ; Use unsigned compare
SSEfwdCopyLongAlignedAMDOpteronOverlapped:
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                test    ecx, 7
                jz      short SSEfclLoopNoResidueAMDOpteron
SSEfclLoopAMDOpteron:
                movq    xmm7, qword ptr [esi+ecx]
                movq    qword ptr [edi+ecx], xmm7
                add     ecx, 8
                cmp     ecx, -8
                jbe     short SSEfclLoopAMDOpteron   ; Use unsigned compare
                neg     ecx
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                align   16
SSEfclLoopNoResidueAMDOpteron:
                movq    xmm7, qword ptr [esi+ecx]
                movq    qword ptr [edi+ecx], xmm7
                add     ecx, 8
                jl      short SSEfclLoopNoResidueAMDOpteron
                ret

		; you can not call this function if your arraycopy length is less than 32
                align 16
fclMovDQUAMDOpteron:       
				; save temp regs
				sub esp, 32
				movdqu oword ptr [esp], xmm0
				movdqu oword ptr [esp + 16], xmm1
				push eax
				
				mov eax, esi
				sub eax, edi
				and eax, 31
                jz fclMovDQUAMDOpteronNoOverlap
				pop eax
				add esp, 32
                jmp SSEfwdCopyLongAlignedAMDOpteronOverlapped
				
fclMovDQUAMDOpteronNoOverlap:
				; align destination and adjust size
				mov eax, ecx;
				sub ecx, edi;
				sub ecx, eax;
				and ecx, 31;
				sub eax, ecx;
                jle fclMovDQUEndBytesAMDOpteron;
				add edi, ecx;
				add esi, ecx;
				neg ecx
				movdqu xmm0, oword ptr [esi+ecx];
				movdqu xmm1, oword ptr [esi+ecx+16];
				movdqu oword ptr [edi+ecx], xmm0;
				movdqu oword ptr [edi+ecx+16], xmm1;
				mov ecx, eax;
				and eax, 31;
				shr ecx, 5;		
                jz fclMovDQUEndBytesAMDOpteron;
				sub edi, esi;
fclMovDQULoopAMDOpteron:
				movdqu xmm0, oword ptr [esi];
				movdqu oword ptr [edi+esi], xmm0;
				movdqu xmm1, oword ptr [esi+16];
				movdqu oword ptr [edi+esi+16], xmm1;
				add esi, 32;
				dec ecx;
                jnz fclMovDQULoopAMDOpteron;
				add edi, esi;
fclMovDQUEndBytesAMDOpteron:
				add ecx, eax;
				sub ecx, 32
				movdqu xmm0, oword ptr [esi+ecx];
				movdqu xmm1, oword ptr [esi+ecx+16];
				movdqu oword ptr [edi+ecx], xmm0;
				movdqu oword ptr [edi+ecx+16], xmm1;                
				; restore temp regs
				pop eax
				movdqu xmm0, oword ptr [esp]
				movdqu xmm1, oword ptr [esp + 16]
				add esp, 32
				ret        		
_SSEforwardArrayCopyAMDOpteron endp

                align   16
;
; A c-style memcpy based on SSE with element size known to be 2 bytes
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_SSEforwardHalfWordArrayCopy PROC NEAR
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     LSSEfwdHalfWordCopyLong   ; Use unsigned compare

                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret
_SSEforwardHalfWordArrayCopy endp


                align   16
;
; A c-style memcpy with no assumptions on the element size
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_forwardArrayCopy PROC NEAR
LforwardArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     short LfwdCopyLong   ; Use unsigned compare

                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret
_forwardArrayCopy endp
;
                align   16
;
; Long forward copy. The minimum supported copy length is 12 bytes.
;
; ecx has length of copy in bytes, 12 is the minmimum.
; esi has source address
; edi has destination address
_fwdCopyLong PROC NEAR
LfwdCopyLong:                           ; Entry when target is byte aligned
                test    edi, 3          ; Perform required alignemnt
                jz      short fwdCopyLongAligned
                test    edi, 1
                jz      short fclAlign2
                sub     ecx, 1
                movsb
LfwdHalfWordCopyLong:                   ; Entry when target is two byte aligned
                test    edi, 2
                jz      short fwdCopyLongAligned
fclAlign2:
                sub     ecx, 2
                movsw
fwdCopyLongAligned:
                cmp     ecx, MOV_THRESHOLD
                jae     short fclRepMov   ; Use unsigned compare
                push    eax
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                test    ecx, 7
                jz      short fclLoopNoResidue
fclLoop:
                mov     eax, dword ptr [esi+ecx]
                mov     dword ptr [edi+ecx], eax
                mov     eax, dword ptr [esi+ecx+4]
                mov     dword ptr [edi+ecx+4], eax
                add     ecx, 8
                cmp     ecx, -8
                jbe     short fclLoop   ; Use unsigned compare
                neg     ecx
                pop     eax
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                align   16
fclLoopNoResidue:
                mov     eax, dword ptr [esi+ecx]
                mov     dword ptr [edi+ecx], eax
                mov     eax, dword ptr [esi+ecx+4]
                mov     dword ptr [edi+ecx+4], eax
                add     ecx, 8
                jl      short fclLoopNoResidue
                pop     eax
                ret

                align   16
fclRepMov:
                test    ecx, 1
                jnz     short fclOddLength
                shr     ecx, 2              ; also sets CF flag if count is xxx2
                rep     movsd
                jb      short fclFinishEven ; test CF flag
                ret                         ; No rsidue.
fclFinishEven:                              ; Residue is 2 bytes.
                movsw
                ret

                align   16
fclOddLength:
                shr     ecx, 2              ; also sets CF flag if count is xxx2 or xxx3
                rep     movsd
                jae     short fclFinishOdd  ; test CF flag
                movsw                       ; Residue is 3 bytes.
fclFinishOdd:                               ; Residue is now 1 byte.
                movsb
                ret
_fwdCopyLong endp
;
                align   16
;
; A c-style memmove where the element size is known to be 2 bytes
; and copy direction is not known.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_halfWordArrayCopy PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardHalfWordArrayCopy
                jmp     short LforwardHalfWordArrayCopy
_halfWordArrayCopy endp
;
                align   16
;
; A c-style memcpy with element size known to be 2 bytes
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_forwardHalfWordArrayCopy PROC NEAR
LforwardHalfWordArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     LfwdHalfWordCopyLong   ; Use unsigned compare

                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                ; --------------------------------------------------------
                ; NOTE:
                ; These copy sequences are shared by all forward copy
                ; procedures. They are placed here because they are nearest
                ; to all invokers.
                ; --------------------------------------------------------
                ; Forward copy table section for length modulus 4 = 0
                ; --------------------------------------------------------
fwdCopy32:      mov     ecx, dword ptr [esi-32]
                mov     dword ptr [edi-32], ecx
fwdCopy28:      mov     ecx, dword ptr [esi-28]
                mov     dword ptr [edi-28], ecx
fwdCopy24:      mov     ecx, dword ptr [esi-24]
                mov     dword ptr [edi-24], ecx
fwdCopy20:      mov     ecx, dword ptr [esi-20]
                mov     dword ptr [edi-20], ecx
fwdCopy16:      mov     ecx, dword ptr [esi-16]
                mov     dword ptr [edi-16], ecx
fwdCopy12:      mov     ecx, dword ptr [esi-12]
                mov     dword ptr [edi-12], ecx
fwdCopy8:       mov     ecx, dword ptr [esi-8]
                mov     dword ptr [edi-8], ecx
fwdCopy4:       mov     ecx, dword ptr [esi-4]
                mov     dword ptr [edi-4], ecx
fwdCopy0:       ret

                ; --------------------------------------------------------
                ; Forward copy table section for length modulus 4 = 2
                ; --------------------------------------------------------
fwdCopy30:      mov     ecx, dword ptr [esi-30]
                mov     dword ptr [edi-30], ecx
fwdCopy26:      mov     ecx, dword ptr [esi-26]
                mov     dword ptr [edi-26], ecx
fwdCopy22:      mov     ecx, dword ptr [esi-22]
                mov     dword ptr [edi-22], ecx
fwdCopy18:      mov     ecx, dword ptr [esi-18]
                mov     dword ptr [edi-18], ecx
fwdCopy14:      mov     ecx, dword ptr [esi-14]
                mov     dword ptr [edi-14], ecx
fwdCopy10:      mov     ecx, dword ptr [esi-10]
                mov     dword ptr [edi-10], ecx
fwdCopy6:       mov     ecx, dword ptr [esi-6]
                mov     dword ptr [edi-6], ecx
fwdCopy2:       movzx   ecx, word ptr [esi-2]
                mov     word ptr [edi-2], cx
                ret
_forwardHalfWordArrayCopy endp
;
; aggressive general direction copies

                align   16
_arrayCopyAggressive PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardArrayCopy
                jmp     LSSEfwdCopyAggressiveStart
_arrayCopyAggressive ENDP

                align   16
_wordArrayCopyAggressive PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardWordArrayCopy
                jmp     LSSEfwdCopyAggressiveStart
_wordArrayCopyAggressive ENDP

                align   16
_halfWordArrayCopyAggressive PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardHalfWordArrayCopy
                jmp     LSSEfwdCopyAggressiveStart
_halfWordArrayCopyAggressive ENDP

                align   16
;
; A c-style memmove where the element size is known to be a multiple of 4 bytes
; and copy direction is not known.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_wordArrayCopy PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardWordArrayCopy
                jmp     short LforwardWordArrayCopy
_wordArrayCopy ENDP
;
                align   16
;
; A c-style memcpy with element size known to be a multiple of 4 bytes
; starting from low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_forwardWordArrayCopy PROC NEAR
LforwardWordArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, LONG_COPY_THRESHOLD
                jb      short fwacCopyShort   ; Use unsigned compare

                ; --------------------------------------------------------
                ; Forward 4 byte copy long, copy length is a multiple of 4.
                ; --------------------------------------------------------
                cmp     ecx, MOV_THRESHOLD
                jae     short fwacRepMov          ; Use unsigned compare
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                push    eax
                test    ecx, 4
                jz      short fwacLoopNoReside
                mov     eax, dword ptr [esi+ecx]  ; Copy residue bytes first
                mov     dword ptr [edi+ecx], eax
                add     ecx, 4
fwacLoopNoReside:
                mov     eax, dword ptr [esi+ecx]
                mov     dword ptr [edi+ecx], eax
                mov     eax, dword ptr [esi+ecx+4]
                mov     dword ptr [edi+ecx+4], eax
                add     ecx, 8
                jl      short fwacLoopNoReside
                pop     eax
                ret

                align   16
fwacRepMov:
                shr     ecx, 2
                rep     movsd
                ret

                align   16
fwacCopyShort:
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret
_forwardWordArrayCopy ENDP
;
                align   16
;
; A c-style memcpy with element size not known
; starting from high address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_backwardArrayCopy PROC NEAR
LbackwardArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     LbwdCopyLong   ; Use unsigned compare

                JumpTableHelper eax,ecx,bwdCopyTable
                ; Jump table will do a ret

                ; --------------------------------------------------------
                ; NOTE:
                ; These copy sequences are shared by all forward copy
                ; procedures. They are placed here because they are nearest
                ; to their invokers. Odd size copies are only used for byte
                ; copy procedures.
                ; --------------------------------------------------------
                ; Backward copy table section for length modulus 4 = 1
                ; --------------------------------------------------------
bwdCopy29:      mov     ecx, dword ptr [esi+25]
                mov     dword ptr[edi+25], ecx
bwdCopy25:      mov     ecx, dword ptr [esi+21]
                mov     dword ptr[edi+21], ecx
bwdCopy21:      mov     ecx, dword ptr [esi+17]
                mov     dword ptr[edi+17], ecx
bwdCopy17:      mov     ecx, dword ptr [esi+13]
                mov     dword ptr[edi+13], ecx
bwdCopy13:      mov     ecx, dword ptr [esi+9]
                mov     dword ptr[edi+9], ecx
bwdCopy9:       mov     ecx, dword ptr [esi+5]
                mov     dword ptr[edi+5], ecx
bwdCopy5:       mov     ecx, dword ptr [esi+1]
                mov     dword ptr[edi+1], ecx
bwdCopy1:       movzx   ecx, byte ptr [esi]
                mov     byte ptr [edi], cl
                ret

                ; --------------------------------------------------------
                ; Backward copy table section for length modulus 4 = 3
                ; --------------------------------------------------------
bwdCopy31:      mov     ecx, dword ptr [esi+27]
                mov     dword ptr[edi+27], ecx
bwdCopy27:      mov     ecx, dword ptr [esi+23]
                mov     dword ptr[edi+23], ecx
bwdCopy23:      mov     ecx, dword ptr [esi+19]
                mov     dword ptr[edi+19], ecx
bwdCopy19:      mov     ecx, dword ptr [esi+15]
                mov     dword ptr[edi+15], ecx
bwdCopy15:      mov     ecx, dword ptr [esi+11]
                mov     dword ptr[edi+11], ecx
bwdCopy11:      mov     ecx, dword ptr [esi+7]
                mov     dword ptr[edi+7], ecx
bwdCopy7:       mov     ecx, dword ptr [esi+3]
                mov     dword ptr[edi+3], ecx
bwdCopy3:       movzx   ecx, word ptr [esi+1]
                mov     word ptr [edi+1], cx
                movzx   ecx, byte ptr [esi]
                mov     byte ptr [edi], cl
                ret
_backwardArrayCopy ENDP
;
                align   16
;
; Long backward copy. The minimum supported copy length is 12 bytes.
;
; ecx has length of copy in bytes, 12 is the minmimum.
; esi has source address
; edi has destination address
_bwdCopyLong PROC NEAR
LbwdCopyLong:
                push    eax
                add     edi, ecx
                test    edi, 3                ; Perform required alignemnt
                jz      short bclAligned      ; Check if the end of array is aligned.
                test    edi, 1
                jz      short bclAlign2
                sub     ecx, 1
                sub     edi, 1
                movzx   eax, byte ptr [esi+ecx]
                mov     byte ptr [edi], al
                test    edi, 2
                jz      short bclAligned
bclAlign2:
                sub     ecx, 2
                sub     edi, 2
                movzx   eax, word ptr [esi+ecx]
                mov     word ptr [edi], ax
bclAligned:
                sub     edi, ecx              ; Alignment done, restore edi

                cmp     ecx, MOV_THRESHOLD
                jae     short bclRepMov       ; Use unsigned compare
                sub     ecx, 8
                test    ecx, 7
                jz      short bclLoopNoResidue
bclLoop:
                mov     eax, dword ptr [esi+ecx+4]
                mov     dword ptr [edi+ecx+4], eax
                mov     eax, dword ptr [esi+ecx]
                mov     dword ptr [edi+ecx], eax
                sub     ecx, 8
                cmp     ecx, 8
                jae     short bclLoop         ; Use unsigned compare
                add     ecx, 8                ; Recover residue count
                pop     eax
                JumpTableHelper eax,ecx,bwdCopyTable
                ; Jump table will do a ret

                align   16
bclLoopNoResidue:
                mov     eax, dword ptr [esi+ecx+4]
                mov     dword ptr [edi+ecx+4], eax
                mov     eax, dword ptr [esi+ecx]
                mov     dword ptr [edi+ecx], eax
                sub     ecx, 8
                jge     short bclLoopNoResidue
                pop     eax
                ret

                align   16
bclRepMov:
                pop     eax
                std
                lea     esi, [esi+ecx-4]
                lea     edi, [edi+ecx-4]
                test    ecx, 1
                jnz     short bclOddLength
                shr     ecx, 2                ; also sets CF flag if count is xxx2
                rep     movsd
                jb      short bclFinishEven   ; test CF flag
                cld                           ; No residue.
                ret
bclFinishEven:                                ; Residue is 2 bytes.
                movzx   ecx, word ptr [esi+2]
                mov     word ptr [edi+2], cx
                cld
                ret

                align   16
bclOddLength:
                shr     ecx, 2                ; also sets CF flag if count is xxx2 or xxx3
                rep     movsd
                jb      short bclResidue3     ; test CF flag
                movzx   ecx, byte ptr [esi+3] ; Residue is 1 byte.
                mov     byte ptr [edi+3], cl
                cld
                ret

bclResidue3:                                  ; Resude is 3 bytes.
                movzx   ecx, word ptr [esi+2]
                mov     word ptr [edi+2], cx
                movzx   ecx, byte ptr [esi+1]
                mov     byte ptr [edi+1], cl
                cld
                ret
_bwdCopyLong endp
;
                align   16
;
; A c-style memcpy with element size known to be a multiple of 2 bytes
; starting from high address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_backwardHalfWordArrayCopy PROC NEAR
LbackwardHalfWordArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, ALIGN_THRESHOLD
                jae     LbwdCopyLong   ; Use unsigned compare

                JumpTableHelper eax,ecx,bwdCopyTable
                ; Jump table will do a ret
                ; --------------------------------------------------------
                ; NOTE:
                ; These copy sequences are shared by all forward copy
                ; procedures. They are placed here because they are nearest
                ; to their invokers.
                ; --------------------------------------------------------
                ; Backward copy table section for length modulus 4 = 2
                ; --------------------------------------------------------
bwdCopy30:      mov     ecx, dword ptr [esi+26]
                mov     dword ptr[edi+26], ecx
bwdCopy26:      mov     ecx, dword ptr [esi+22]
                mov     dword ptr[edi+22], ecx
bwdCopy22:      mov     ecx, dword ptr [esi+18]
                mov     dword ptr[edi+18], ecx
bwdCopy18:      mov     ecx, dword ptr [esi+14]
                mov     dword ptr[edi+14], ecx
bwdCopy14:      mov     ecx, dword ptr [esi+10]
                mov     dword ptr[edi+10], ecx
bwdCopy10:      mov     ecx, dword ptr [esi+6]
                mov     dword ptr[edi+6], ecx
bwdCopy6:       mov     ecx, dword ptr [esi+2]
                mov     dword ptr[edi+2], ecx
bwdCopy2:       movzx   ecx, word ptr [esi]
                mov     word ptr [edi], cx
                ret

                ; --------------------------------------------------------
                ; Backward copy table section for length modulus 4 = 0
                ; --------------------------------------------------------
bwdCopy32:      mov     ecx, dword ptr [esi+28]
                mov     dword ptr [edi+28], ecx
bwdCopy28:      mov     ecx, dword ptr [esi+24]
                mov     dword ptr [edi+24], ecx
bwdCopy24:      mov     ecx, dword ptr [esi+20]
                mov     dword ptr [edi+20], ecx
bwdCopy20:      mov     ecx, dword ptr [esi+16]
                mov     dword ptr [edi+16], ecx
bwdCopy16:      mov     ecx, dword ptr [esi+12]
                mov     dword ptr [edi+12], ecx
bwdCopy12:      mov     ecx, dword ptr [esi+8]
                mov     dword ptr [edi+8], ecx
bwdCopy8:       mov     ecx, dword ptr [esi+4]
                mov     dword ptr [edi+4], ecx
bwdCopy4:       mov     ecx, dword ptr [esi]
                mov     dword ptr [edi], ecx
bwdCopy0:       ret
_backwardHalfWordArrayCopy ENDP
;
                align   16
;
; A c-style memcpy with element size known to be a multiple of 4 bytes
; starting from high address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_backwardWordArrayCopy PROC NEAR
LbackwardWordArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, LONG_COPY_THRESHOLD
                jb      short bwacShort   ; Use unsigned compare

                ; --------------------------------------------------------
                ; Forward 4 byte copy long, copy length a multiple of 4.
                ; --------------------------------------------------------
                cmp     ecx, MOV_THRESHOLD
                jae     short bwacRepMov   ; Use unsigned compare
                push    eax
                test    ecx, 4
                jz      short bwacNoReside
                sub     ecx, 4
                mov     eax, dword ptr [esi+ecx]  ; Copy residue bytes first
                mov     dword ptr [edi+ecx], eax
bwacNoReside:
                sub     ecx, 8
bwacLoopNoReside:
                mov     eax, dword ptr [esi+ecx+4]
                mov     dword ptr [edi+ecx+4], eax
                mov     eax, dword ptr [esi+ecx]
                mov     dword ptr [edi+ecx], eax
                sub     ecx, 8
                jge     short bwacLoopNoReside
                pop     eax
                ret

                align   16
bwacRepMov:
                std
                lea     esi, [esi+ecx-4]
                lea     edi, [edi+ecx-4]
                shr     ecx, 2
                rep     movsd
                cld
                ret

                align   16
bwacShort:
                JumpTableHelper eax,ecx,bwdCopyTable
                ; Jump table will do a ret
_backwardWordArrayCopy ENDP
;
                align   16
;
; Source and destination operands can be mutually 16 byte aligned and
; the copy length is unknown. Starting from low address element. Minimum
; supported copy length is 31 bytes!
; ecx has length of copy in bytes >= 31
; esi has source address
; edi has destination address
_forwardSSEArrayCopyNoAlignCheck   PROC NEAR
                ; Caller has promised both source and target can be 16B aligned.
                push    eax
                mov     eax, edi
                neg     eax       ; Convert from mis-alignment to to padding needed
                and     eax, 15   ; Number of padding copies to 16 byte align.
                sub     ecx, eax  ; Adjust ecx by the padding needed.
                JumpTableHelper eax,eax,fsacAlignTable
                ; 16 byte alignment jump table.
fsacPad11:      movsb
fsacPad10:      movsw
                jmp     short fsacPad8
fsacPad7:       movsb
fsacPad6:       movsw
                jmp     short fsacPad4
fsacPad3:       movsb
fsacPad2:       movsw
                jmp     short fsacAligned
fsacPad13:      movsb
                jmp     short fsacPad12
fsacPad9:       movsb
                jmp     short fsacPad8
fsacPad5:       movsb
                jmp     short fsacPad4
fsacPad1:       movsb
                jmp     short fsacAligned
fsacPad15:      movsb
fsacPad14:      movsw
fsacPad12:      movsd
fsacPad8:       movsd
fsacPad4:       movsd
fsacAligned:    pop     eax        ; 16B alignment done.

                jmp     LforwardSSEArrayCopyAligned
_forwardSSEArrayCopyNoAlignCheck   ENDP
;
                align   16
;
; Arraycopy operation with no assumptions on the element size or
; required copy direction.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_SSEArrayCopy  PROC NEAR
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      LbackwardArrayCopy
                jmp     short LforwardSSEArrayCopy
_SSEArrayCopy ENDP
;
                align   16
if USE_SSE3
;
; Forward direction memory copy using SSE2/3 16 bytes moves
; when both size and alignment requirements are met. Starting from
; low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
_forwardSSEArrayCopy   PROC NEAR
LforwardSSEArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, 8
                jbe     short fsacTinyCopy     ; Use unsigned compare
                test    edi, 3                 ; Perform required alignemnt
                jz      short fsacAlign4
                test    edi, 1
                jz      short fsacAlign2
                sub     ecx, 1
                movsb
                test    edi, 2
                jz      short fsacAlign4
fsacAlign2:
                sub     ecx, 2
                movsw
fsacAlign4:                                    ; 4 byte alignment done
                cmp     ecx, FWD_SSE_THRESHOLD ; Min is 16 + room to 16b align
                jae     fsacUseSSECopy         ; Use unsigned compare
fsacTinyCopy:
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

fsacUseSSECopy:
                ; --------------------------------------------------------
                ; Forward byte SSE3 copy long.
                ; --------------------------------------------------------
fsacCopyLong:
                test    edi, 12             ; Upgrade from 4b to 16b alignment
                jz      short fsacAlign16
                test    edi, 4
                jz      short fsacAlign8
                sub     ecx, 4
                movsd
                test    edi, 8
                jz      short fsacAlign16
fsacAlign8:
                sub     ecx, 8
                movsd
                movsd
fsacAlign16:                                 ; 16 byte alignment done
                sub     edi, esi
                test    edi, 15 ; Test if both source and target can be 16 byte aligned
                lea     edi, [edi+esi]
                jz      short fsacBothAligned ; Both source and target are 16 byte aligned.

                sub     esp, 8
                movq    qword ptr [esp], xmm0
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                test    ecx, 15
                jz      short fsacLoopNoResidue
fsacLoop:
                dd      04F00FF2h               ; lddqu xmm0, oword ptr [esi+ecx]
                db      031h
                movdqa  oword ptr [edi+ecx], xmm0
                add     ecx, 16
                cmp     ecx, -16
                jbe     short fsacLoop          ; Use unsigned compare
                movq    xmm0, qword ptr [esp]
                add     esp, 8
                neg     ecx
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                align   16
fsacLoopNoResidue:
                dd      04F00FF2h               ; lddqu xmm0, oword ptr [esi+ecx]
                db      031h
                movdqa  oword ptr [edi+ecx], xmm0
                add     ecx, 16
                jl      short fsacLoopNoResidue
                movq    xmm0, qword ptr [esp]
                add     esp, 8
                ret

                align   16
fsacBothAligned:                                ; Both addresses are 16 byte aligned
LforwardSSEArrayCopyAligned:
                sub     esp, 8
                movq    qword ptr [esp], xmm0
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                test    ecx, 15
                jz      short fsacBothAlignedLoopNoResidue
fsacBothAlignedLoop:
                movdqa  xmm0, oword ptr [esi+ecx]
                movdqa  oword ptr [edi+ecx], xmm0
                add     ecx, 16
                cmp     ecx, -16
                jbe     short fsacBothAlignedLoop   ; Use unsigned compare
                neg     ecx
                movq    xmm0, qword ptr [esp]
                add     esp, 8
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                align 16
fsacBothAlignedLoopNoResidue:
                movdqa  xmm0, oword ptr [esi+ecx]
                movdqa  oword ptr [edi+ecx], xmm0
                add     ecx, 16
                jl      short fsacBothAlignedLoopNoResidue
                movq    xmm0, qword ptr [esp]
                add     esp, 8
                ret
_forwardSSEArrayCopy   ENDP
else    ; USE_SSE3
;
;
; Forward direction memory copy using SSE2 16 bytes moves
; when both size and mutual alignment requirements are met. Starting from
; low address element.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
;
 _forwardSSEArrayCopy   PROC NEAR
LforwardSSEArrayCopy:
IFDEF DEBUGSTATS
                call    __arrayCopyStats
ENDIF
                cmp     ecx, 8
                jbe     short fsacTinyCopy     ; Use unsigned compare
                test    edi, 3                 ; Perform required alignemnt
                jz      short fsacAlign4
                test    edi, 1
                jz      short fsacAlign2
                sub     ecx, 1
                movsb
                test    edi, 2
                jz      short fsacAlign4
fsacAlign2:
                sub     ecx, 2
                movsw
fsacAlign4:                                    ; 4 byte alignment done
                cmp     ecx, FWD_SSE_THRESHOLD ; Min is 16 + room to 16b align
                jae     short fsacTrySSECopy   ; Use unsigned compare
fsacTinyCopy:
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

fsacTrySSECopy:
                sub     edi, esi
                test    edi, 15                ; Can source and target be 16 byte aligned?
                lea     edi, [edi+esi]
                ; When not alignable use non-SSE forward copy long.
                jnz     fwdCopyLongAligned

                ; --------------------------------------------------------
                ; Forward byte SSE2 copy long.
                ; --------------------------------------------------------
fsacCopyLong:
                test    edi, 12             ; Upgrade from 4b to 16b alignment
                jz      short fsacAlign16
                test    edi, 4
                jz      short fsacAlign8
                sub     ecx, 4
                movsd
                test    edi, 8
                jz      short fsacAlign16
fsacAlign8:
                sub     ecx, 8
                movsd
                movsd
fsacAlign16:                                ; 16 byte alignment done
LforwardSSEArrayCopyAligned:
                sub     esp, 8
                movq    qword ptr [esp], xmm0
                lea     esi, [esi+ecx]
                lea     edi, [edi+ecx]
                neg     ecx
                test    ecx, 15
                jz      short fsacLoopNoResidue
fsacLoop:
                movdqa  xmm0, oword ptr [esi+ecx]
                movdqa  oword ptr [edi+ecx], xmm0
                add     ecx, 16
                cmp     ecx, -16
                jbe     short fsacLoop   ; Use unsigned compare
                neg     ecx
                movq    xmm0, qword ptr [esp]
                add     esp, 8
                JumpTableHelper eax,ecx,fwdCopyTable
                ; Jump table will do a ret

                align   16
fsacLoopNoResidue:
                movdqa  xmm0, oword ptr [esi+ecx]
                movdqa  oword ptr [edi+ecx], xmm0
                add     ecx, 16
                jl      short fsacLoopNoResidue
                movq    xmm0, qword ptr [esp]
                add     esp, 8
                ret
_forwardSSEArrayCopy   ENDP
endif   ; USE_SSE3
;
;               align   16
;
; This helper slots are not used can be reused for something else.
;
_shortArrayCopy        PROC NEAR
                int     3             ; This procedure is never called
_shortArrayCopy        ENDP
;
;               align   16
;
; This helper slots are not used can be reused for something else.
;
_forwardArrayCopy2     PROC NEAR
                int     3             ; This procedure is never called
_forwardArrayCopy2     ENDP

                _TEXT ends
