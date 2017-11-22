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

ifndef TR_HOST_64BIT
                include jilconsts.inc
                include x/i386/runtime/IA32ArrayCopy.inc

J9TR_ObjectColorBlack equ 03h

               _TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
                align   16
IFDEF DEBUGSTATS
__arrayCopyStats PROC NEAR;
IFDEF RECORD_ARRAYCOPY_INFO
                ; Record array info outputs the source and target addresses and
                ; the number of bytes copied to a file. This level of instumentation
                ; is intrusive and creates a very large ammount of data.
                ;
                ; struct arraycopy_info
                ;    {
                ;    unsigned int sourceAddress;  unsigned int targetAddress;
                ;    unsigned int length;
                ;    } arraycopy_info
                ;
                ; extern "C" void record_arraycopy_info(unsigned int src, unsigned int tgt, unsigned int size)
                ;   { /* Squished example of a jit arraycopy data writer. */
                ;   static FILE * fptr = 0;   static char buf[BUFSIZ];
                ;   struct aci { unsigned int src; unsigned int tgt; unsigned int size; } aci;
                ;   if (fptr == 0)  { fptr = fopen("arrayInfoDetails.bin", "wb"); setbuf(fptr, buf); }
                ;   ai.src = src; ai.tgt = tgt; ai.size = size;
                ;   fwrite(&ai, sizeof ai, 1, fptr);
                ;   }
                ExternHelper record_arraycopy_info ; C linkage call
                push    eax    ; save volatile reg
                push    edx    ; save volatile reg
                push    ecx    ; save volatile reg
                push    ecx
                push    edi
                push    esi
                CallHelper record_arraycopy_info
                add     esp, 12
                pop     ecx    ; restore volatile reg
                pop     edx    ; restore volatile reg
                pop     eax    ; restore volatile reg
ENDIF
                sub     edi, esi
                cmp     edi, ecx       ; Determine copy direction required
                lea     edi, [edi+esi]
                jb      short moveStatBwd
                ; Forward direction array copy
                add     dword ptr[_copyStats + statMoveFwd], 1
                jmp     short moveStatLength
moveStatBwd:    ; Backward direction array copy
                add     dword ptr[_copyStats + statMoveBwd], 1
moveStatLength:
                cmp     ecx, 64
                jg      short moveStatLong
                add     dword ptr[ecx * 4 + _copyStats + statMoveShort], 1
                ret
moveStatLong:
                add     dword ptr[_copyStats + statMoveLong], 1
                ret
__arrayCopyStats endp
ENDIF
                _TEXT ends

IFDEF DEBUG
                _DATA SEGMENT PARA USE32 PUBLIC 'DATA'

                public _copyStats

;
; Array copy statistics.
;
; struct copyStats
;    {
;    unsigned int shortCount[65];        // Count of copies for 0-64 byte lengths
;    unsigned int longCount;             // Count of copies greter than 64 bytes
;    unsigned int fwdCount;              // Forward direction used (default direction)
;    unsigned int bwdCount;              // Backward direction needed (only if overlap)
;    unsigned int arrayStoreCheck;       // Array-store check count
;    unsigned int writeBarrierStorCheck; // Write barrier check count
;    unsigned int bothCheck;             // Both array-stopr and write barrier check count
;    } copyStats;
;
_copyStats:
statMoveShort     equ   0
                dd      65 DUP (0)   ; array copy lengths of 0 - 64
statMoveLong      equ   260
                dd      0            ; array copy length > 64
statMoveFwd       equ   264
                dd      0            ; forward direction array copy default
statMoveBwd       equ   268
                dd      0            ; backward direction array copy required
statAS            equ   272
                dd      0            ; elements array-store checked
statWrtBar        equ   276
                dd      0            ; elements write-barrier-store checked
statASWrtBar      equ   280
                dd      0            ; elements both-checked
          _DATA ends
ENDIF
else            ; TR_HOST_64BIT
_DATA           segment para 'DATA'
IA32ArrayCopy:
                db      00h
_DATA           ends

endif           ; TR_HOST_64BIT

                end
