; Copyright (c) 2000, 2018 IBM Corp. and others
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

%include "jilconsts.inc"

segment .text

   DECLARE_GLOBAL  arrayTranslateTRTO
   DECLARE_GLOBAL  arrayTranslateTROTNoBreak
   DECLARE_GLOBAL  arrayTranslateTROT

   align 16

   ; pseudocode:
   ; int i;
   ; for (i = 0; i < N; i++)
   ;   if (chararray[i] && DX != 0) break; //DX is the register
   ;   else bytearray[i] = (byte) chararray[i])
   ; return i
   arrayTranslateTRTO:                      ;TO stands for Two bytes to One byte
   %ifdef TR_HOST_64BIT
      mov ecx, ecx
   %endif
      xor  rax, rax
      cmp  rcx, 8
      jl   byteresidualTO
      movd xmm1, edx
      pshufd xmm1, xmm1, 0
      cmp  rcx, 16
      jl   eightcharsTO
   sixteencharsTO:
      movdqu xmm2, [rsi+2*rax]
      ptest xmm2, xmm1          ; SSE4.1 instruction
      jnz  failedloopTO
      movdqu xmm3, [rsi+2*rax+16]
      ptest xmm3, xmm1          ; SSE4.1 instruction
      jnz  eightcharsTO
      packuswb xmm2, xmm3
      movdqu oword [rdi+rax], xmm2
      sub  rcx, 16
      add  rax, 16
      cmp  rcx, 16
      jge  sixteencharsTO
      cmp  rcx, 8
      jl   byteresidualTO
   eightcharsTO:
      movdqu xmm2, [rsi+2*rax]
      ptest xmm2, xmm1          ; SSE4.1 instruction
      jnz  failedloopTO
      packuswb xmm2, xmm1       ; only the first 8 bytes of xmm2 are meaningful
      movq qword [rdi+rax], xmm2
      sub  rcx, 8
      add  rax, 8
   byteresidualTO:
      and rcx, rcx
      je  doneTO
   failedloopTO:
      mov  bx, word [rsi+2*rax]
      test bx, dx
      jnz  doneTO
      mov  byte [rdi+rax], bl
      inc  rax
      dec  rcx
      jnz  failedloopTO
   doneTO:   ;EAX is result register
      ret

   ; pseudocode:
   ; int i;
   ; for (i = 0; i < N; i++)
   ;   chararray[i] = (char) bytearray[i];
   arrayTranslateTROTNoBreak:               ;OT stands for One byte to Two bytes
   %ifdef TR_HOST_64BIT
      mov ecx, ecx
   %endif
      xor    rax, rax
      xorps  xmm3, xmm3
      cmp    rcx, 16
      jl     qwordResidualOTNoBreak

   sixteencharsOTNoBreak:
      movdqu  xmm1, [rsi+rax]
      movdqu  xmm2, xmm1
      punpcklbw xmm1, xmm3
      punpckhbw xmm2, xmm3
      movdqu [rdi+rax*2], xmm1
      movdqu [rdi+rax*2+16], xmm2
      sub rcx, 16
      add rax, 16
      cmp rcx, 16
      jge sixteencharsOTNoBreak

   slideWindowResidualOTNoBreak:
      test rcx, rcx
      jz doneOTNoBreak
      sub rax, 16
      add rax, rcx
      mov rcx, 16
      jmp sixteencharsOTNoBreak

   doneOTNoBreak:
      ret

   qwordResidualOTNoBreak:
      bt rcx, 3
      jnc dwordResidualOTNoBreak
      movq xmm1, qword [rsi+rax]
      punpcklbw xmm1, xmm3
      movdqu [rdi+rax*2], xmm1
      add rax, 8
      sub rcx, 8

   dwordResidualOTNoBreak:
      bt rcx, 2
      jnc byteResidualOTNoBreak
      movd xmm1, dword [rsi+rax]
      punpcklbw xmm1, xmm3
      movq qword [rdi+rax*2], xmm1
      add rax, 4
      sub rcx, 4

   byteResidualOTNoBreak:
      test rcx, rcx
      jz doneOTNoBreak
      xor  bx, bx

   failedloopOTNoBreak:
      mov  bl, [rsi+rax]
      mov  [rdi+rax*2], bx
      inc  rax
      dec  rcx
      jnz  failedloopOTNoBreak
      jmp  doneOTNoBreak



   ; pseudocode:
   ; int i;
   ; for (i = 0; i < N; i++)
   ;   if (bytearray[i] < 0) break;
   ;   else chararray[i] = (char) bytearray[i];
   ; return i;
   arrayTranslateTROT:                      ;OT stands for One byte to Two bytes
   %ifdef TR_HOST_64BIT
      mov ecx, ecx
   %endif
      xor    rax, rax
      xorps  xmm3, xmm3
      cmp    rcx, 16
      jl     qwordResidualOT

   sixteencharsOT:
      movdqu  xmm1, [rsi+rax]
      pmovmskb ebx, xmm1
      test ebx, ebx
      jnz    computeNewLengthOT
      movdqu  xmm2, xmm1
      punpcklbw xmm1, xmm3
      punpckhbw xmm2, xmm3
      movdqu [rdi+rax*2], xmm1
      movdqu [rdi+rax*2+16], xmm2
      sub rcx, 16
      add rax, 16
      cmp rcx, 16
      jge sixteencharsOT

   slideWindowResidualOT:
      test rcx, rcx
      jz doneOT
      sub rax, 16
      add rax, rcx
      mov rcx, 16
      jmp sixteencharsOT

   doneOT:
      ret

   computeNewLengthOT:
      bsf ebx, ebx
   %ifdef TR_HOST_64BIT
      mov ebx, ebx
   %endif
      mov rcx, rbx
      cmp rax, 16
      jge slideWindowResidualOT

   qwordResidualOT:
      bt rcx, 3
      jnc dwordResidualOT
      movq xmm1, qword [rsi+rax]
      pmovmskb ebx, xmm1
      test ebx, ebx
      jnz computeNewLengthOT
      punpcklbw xmm1, xmm3
      movdqu [rdi+rax*2], xmm1
      add rax, 8
      sub rcx, 8

   dwordResidualOT:
      bt rcx, 2
      jnc byteResidualOT
      movd xmm1, dword [rsi+rax]
      pmovmskb ebx, xmm1
      test ebx, ebx
      jnz computeNewLengthOT
      punpcklbw xmm1, xmm3
      movq qword [rdi+rax*2], xmm1
      add rax, 4
      sub rcx, 4

   byteResidualOT:
      test rcx, rcx
      jz doneOT
      xor  bx, bx

   failedloopOT:
      mov  bl, [rsi+rax]
      test bl, 80h
      jnz  doneOT
      mov  [rdi+rax*2], bx
      inc  rax
      dec  rcx
      jnz  failedloopOT
      jmp doneOT

ret
