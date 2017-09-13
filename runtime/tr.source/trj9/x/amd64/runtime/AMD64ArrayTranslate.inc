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

_TEXT           segment para 'CODE'

                public  _arrayTranslateTRTO
                public  _arrayTranslateTROTNoBreak
                public  _arrayTranslateTROT
                align 16


; pseudocode:
; int i;
; for (i = 0; i < N; i++)
;   if (chararray[i] && DX != 0) break; //DX is the register
;   else bytearray[i] = (byte) chararray[i])
; return i
_arrayTranslateTRTO PROC                      ;TO stands for Two bytes to One byte
		XOR  RAX, RAX
		CMP  RCX, 8
		JL   byteresidualTO
		MOVD xmm1, EDX
		MOVD xmm2, EDX
		SHUFPS xmm1, xmm2, 0
		CMP  RCX, 16
		JL   eightcharsTO
sixteencharsTO:
		MOVUPS xmm2, [RSI+2*rax]  		
		PTEST xmm2, xmm1          ; SSE4.1 instruction
		jnz  failedloopTO
		MOVUPS xmm3, [RSI+2*rax+16]  	
		PTEST xmm3, xmm1          ; SSE4.1 instruction
		jnz  eightcharsTO
		packuswb xmm2, xmm3
		MOVUPS oword ptr [rdi+rax], xmm2
		SUB  RCX, 16
		ADD  RAX, 16
		CMP  RCX, 16
		jge  sixteencharsTO
		CMP  RCX, 8
		JL   byteresidualTO
eightcharsTO:
		MOVUPS xmm2, [RSI+2*rax]  		
		PTEST xmm2, xmm1          ; SSE4.1 instruction
		jnz  failedloopTO
		packuswb xmm2, xmm1       ; only the first 8 bytes of xmm2 are meaningful
		movq qword ptr [rdi+rax], xmm2
		SUB  RCX, 8
		ADD  RAX, 8
byteresidualTO:
        AND RCX, RCX
        je  doneTO
failedloopTO:
        MOV  BX, word ptr [rsi+2*rax]
        TEST BX, DX
        jnz  doneTO
        mov  byte ptr [rdi+rax],bl
        INC  RAX
        DEC  RCX
        jnz  failedloopTO
doneTO:   ;EAX is result register
        ret
_arrayTranslateTRTO endp



; pseudocode:
; int i;
; for (i = 0; i < N; i++)
;   chararray[i] = (char) bytearray[i]
_arrayTranslateTROTNoBreak PROC                      ;OT stands for One byte to Two bytes
	    MOV    RAX, RCX
		CMP    RCX, 8
		JL     byteresidualOTNoBreak
		XORPS  XMM1, XMM1
eightcharsOTNoBreak:
	    MOVQ   XMM2, qword ptr [RSI]
		PUNPCKLBW  XMM2, XMM1		
		MOVUPS [RDI], xmm2
		SUB  RCX, 8

		ADD  RDI, 16
		ADD  RSI, 8	
		CMP  RCX, 8
		jge  eightcharsOTNoBreak

byteresidualOTNoBreak:
        AND RCX, RCX
        je  doneOTNoBreak
        XOR  BX, BX
failedloopOTNoBreak:
        MOV  BL, byte ptr [rsi]
        mov  word ptr [rdi],BX
        ADD  RDI, 2
        INC  RSI
        DEC  RCX
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
		XOR    RAX, RAX
		CMP    RCX, 8
		JL     byteresidualOT
        MOV    EDX, 080808080h
		MOVD   xmm1, EDX
		MOVD   xmm2, EDX
		SHUFPS xmm1, xmm2, 0
        XORPS  xmm3, xmm3
eightcharsOT:
		MOVQ   xmm2, qword ptr [RSI]  		
		PTEST  xmm2, xmm1          ; SSE4.1 instruction
		jnz    byteresidualOT
		PUNPCKLBW  XMM2, XMM3		
		movups [RDI], XMM2
		SUB  RCX, 8
		ADD  RAX, 8

		ADD  RSI, 8
		ADD  RDI, 16	
		CMP  RCX, 8
		jge  eightcharsOT

byteresidualOT:
        AND RCX, RCX
        je  doneOT
        XOR  BX, BX        
failedloopOT:		
        MOV  BL, byte ptr [rsi]
        TEST BL, BL
        js   doneOT
        mov  word ptr [rdi],BX
        INC  RSI
        ADD  RDI, 2
        INC  RAX
        DEC  RCX
        jnz  failedloopOT
doneOT:   ;EAX is result register
        ret
_arrayTranslateTROT endp

_TEXT           ends


