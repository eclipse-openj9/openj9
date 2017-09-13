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

                assume cs:flat,ds:flat,ss:flat

_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

                public  _arrayTranslateTRTO
                public  _arrayTranslateTROTNoBreak
                public  _arrayTranslateTROT
                align   16
;
;

; pseudocode:
; int i;
; for (i = 0; i < N; i++)
;   if (chararray[i] && DX != 0) break; //DX is the register
;   else bytearray[i] = (byte) chararray[i])
; return i
_arrayTranslateTRTO PROC NEAR                   ;TO stands for Two bytes to One byte
		XOR  EAX, EAX
		CMP  ECX, 8
		JL   byteresidualTO
		MOVD xmm1, EDX
		MOVD xmm2, EDX
		SHUFPS xmm1, xmm2, 0
eightcharsTO:
		MOVUPS xmm2, [ESI]  		
		PTEST xmm2, xmm1          ; SSE4.1 instruction
		jnz  failedloopTO
		packuswb xmm2, xmm1       ; only the first 8 bytes of xmm2 are meaningful
		movq qword ptr [edi], xmm2
		SUB  ECX, 8
		ADD  EAX, 8

		ADD  EDI, 8
		ADD  ESI, 16	
		CMP  ECX, 8
		jge  eightcharsTO

byteresidualTO:
        AND ECX, ECX
        je  doneTO
failedloopTO:
        MOV  BX, word ptr [esi]
        TEST BX, DX
        jnz  doneTO
        mov  byte ptr [edi],bl
        INC  EDI
        ADD  ESI, 2
        INC  EAX
        DEC  ECX
        jnz  failedloopTO
doneTO:   ;EAX is result register
        ret
_arrayTranslateTRTO endp


; pseudocode:
; int i;
; for (i = 0; i < N; i++)
;   chararray[i] = (char) bytearray[i]
_arrayTranslateTROTNoBreak PROC                      ;OT stands for One byte to Two bytes
	    MOV    EAX, ECX
		CMP    ECX, 8
		JL     byteresidualOTNoBreak
		XORPS  XMM1, XMM1
eightcharsOTNoBreak:
	    MOVQ   XMM2, qword ptr [ESI]
		PUNPCKLBW  XMM2, XMM1		
		MOVUPS [EDI], xmm2
		SUB  ECX, 8

		ADD  EDI, 16
		ADD  ESI, 8	
		CMP  ECX, 8
		jge  eightcharsOTNoBreak

byteresidualOTNoBreak:
        AND ECX, ECX
        je  doneOTNoBreak
        XOR BX, BX
failedloopOTNoBreak:
        MOV  BL, byte ptr [esi]
        mov  word ptr [edi],BX
        ADD  eDI, 2
        INC  eSI
        DEC  eCX
        jnz  failedloopOTNoBreak
doneOTNoBreak: 
        ret
_arrayTranslateTROTNoBreak endp


; pseudocode:
; int i;
; for (i = 0; i < N; i++)
;   if (bytearray[i] < 0) break;
;   else chararray[i] = (char) bytearray[i]
; return i;
_arrayTranslateTROT PROC                      ;OT stands for One byte to Two bytes
		XOR    EAX, EAX
		CMP    ECX, 8
		JL     byteresidualOT
        MOV    EDX, 080808080h
		MOVD   xmm1, EDX
		MOVD   xmm2, EDX
		SHUFPS xmm1, xmm2, 0
        XORPS  xmm3, xmm3
eightcharsOT:
		MOVQ   xmm2, qword ptr [ESI]  		
		PTEST  xmm2, xmm1          ; SSE4.1 instruction
		jnz    byteresidualOT
		PUNPCKLBW  XMM2, XMM3		
		movups [EDI], XMM2
		SUB  ECX, 8
		ADD  EAX, 8

		ADD  ESI, 8
		ADD  EDI, 16	
		CMP  ECX, 8
		jge  eightcharsOT

byteresidualOT:
        AND ECX, ECX
        je  doneOT
        XOR BX,BX
failedloopOT:		 
        MOV  BL, byte ptr [esi]
        TEST BL, BL
        js   doneOT
        mov  word ptr [edi],BX
        INC  ESI
        ADD  EDI, 2
        INC  EAX
        DEC  ECX
        jnz  failedloopOT
doneOT:   ;EAX is result register
        ret
_arrayTranslateTROT endp



_TEXT           ends

