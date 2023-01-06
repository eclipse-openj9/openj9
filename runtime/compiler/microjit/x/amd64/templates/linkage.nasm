; Copyright (c) 2022, 2022 IBM Corp. and others
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
%include "utils.nasm"

;
; Create templates
;

section .text

template_start saveXMM0Local
   movq [r14 + 0xefbeadde], xmm0
template_end saveXMM0Local

template_start saveXMM1Local
   movq [r14 + 0xefbeadde], xmm1
template_end saveXMM1Local

template_start saveXMM2Local
   movq [r14 + 0xefbeadde], xmm2
template_end saveXMM2Local

template_start saveXMM3Local
   movq [r14 + 0xefbeadde], xmm3
template_end saveXMM3Local

template_start saveXMM4Local
   movq [r14 + 0xefbeadde], xmm4
template_end saveXMM4Local

template_start saveXMM5Local
   movq [r14 + 0xefbeadde], xmm5
template_end saveXMM5Local

template_start saveXMM6Local
   movq [r14 + 0xefbeadde], xmm6
template_end saveXMM6Local

template_start saveXMM7Local
   movq [r14 + 0xefbeadde], xmm7
template_end saveXMM7Local

template_start saveRAXLocal
   mov [r14 + 0xefbeadde], rax
template_end saveRAXLocal

template_start saveRSILocal
   mov [r14 + 0xefbeadde], rsi
template_end saveRSILocal

template_start saveRDXLocal
   mov [r14 + 0xefbeadde], rdx
template_end saveRDXLocal

template_start saveRCXLocal
   mov [r14 + 0xefbeadde], rcx
template_end saveRCXLocal

template_start saveR11Local
   mov [r14 + 0xefbeadde], r11
template_end saveR11Local

template_start movRSPOffsetR11
   mov r11, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end movRSPOffsetR11

template_start movRSPR10
   mov r10, rsp ; Used for allocating space on the stack.
template_end movRSPR10

template_start movR10R14
   mov r14, r10 ; Used for allocating space on the stack.
template_end movR10R14

template_start subR10Imm4
   sub r10, 0x7fffffff ; Used for allocating space on the stack.
template_end subR10Imm4

template_start addR10Imm4
   add r10, 0x7fffffff ; Used for allocating space on the stack.
template_end addR10Imm4

template_start subR14Imm4
   sub r14, 0x7fffffff ; Used for allocating space on the stack.
template_end subR14Imm4

template_start addR14Imm4
   add r14, 0x7fffffff ; Used for allocating space on the stack.
template_end addR14Imm4

template_start saveRBPOffset
   mov [rsp+0xff], rbp ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRBPOffset

template_start saveEBPOffset
   mov [rsp+0xff], ebp ; Used ff as all other labels look valid and 255 will be rare.
template_end saveEBPOffset

template_start saveRSPOffset
   mov [rsp+0xff], rsp ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRSPOffset

template_start saveESPOffset
   mov [rsp+0xff], rsp ; Used ff as all other labels look valid and 255 will be rare.
template_end saveESPOffset

template_start loadRBPOffset
   mov rbp, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRBPOffset

template_start loadEBPOffset
   mov ebp, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadEBPOffset

template_start loadRSPOffset
   mov rsp, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRSPOffset

template_start loadESPOffset
   mov rsp, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadESPOffset

template_start saveEAXOffset
   mov [rsp+0xff], eax ; Used ff as all other labels look valid and 255 will be rare.
template_end saveEAXOffset

template_start saveESIOffset
   mov [rsp+0xff], esi ; Used ff as all other labels look valid and 255 will be rare.
template_end saveESIOffset

template_start saveEDXOffset
   mov [rsp+0xff], edx ; Used ff as all other labels look valid and 255 will be rare.
template_end saveEDXOffset

template_start saveECXOffset
   mov [rsp+0xff], ecx ; Used ff as all other labels look valid and 255 will be rare.
template_end saveECXOffset

template_start saveEBXOffset
   mov [rsp+0xff], ebx ; Used ff as all other labels look valid and 255 will be rare.
template_end saveEBXOffset

template_start loadEAXOffset
   mov eax, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadEAXOffset

template_start loadESIOffset
   mov esi, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadESIOffset

template_start loadEDXOffset
   mov edx, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadEDXOffset

template_start loadECXOffset
   mov ecx, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadECXOffset

template_start loadEBXOffset
   mov ebx, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadEBXOffset

template_start addRSPImm8
   add rsp, 0x7fffffffffffffff ; Used for allocating space on the stack.
template_end addRSPImm8

template_start addRSPImm4
   add rsp, 0x7fffffff ; Used for allocating space on the stack.
template_end addRSPImm4

template_start addRSPImm2
   add rsp, 0x7fff ; Used for allocating space on the stack.
template_end addRSPImm2

template_start addRSPImm1
   add rsp, 0x7f ; Used for allocating space on the stack.
template_end addRSPImm1

template_start je4ByteRel
   je 0xff;
template_end je4ByteRel

template_start jeByteRel
   je 0xff;
template_end jeByteRel

template_start subRSPImm8
   sub rsp, 0x7fffffffffffffff ; Used for allocating space on the stack.
template_end subRSPImm8

template_start subRSPImm4
   sub rsp, 0x7fffffff ; Used for allocating space on the stack.
template_end subRSPImm4

template_start subRSPImm2
   sub rsp, 0x7fff ; Used for allocating space on the stack.
template_end subRSPImm2

template_start subRSPImm1
   sub rsp, 0x7f ; Used for allocating space on the stack.
template_end subRSPImm1

template_start jbe4ByteRel
   jbe 0xefbeadde ; Used for generating jumps to far labels.
template_end jbe4ByteRel

template_start cmpRspRbpDerefOffset
   cmp rsp, [rbp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end cmpRspRbpDerefOffset

template_start loadRAXOffset
   mov rax, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRAXOffset

template_start loadRSIOffset
   mov rsi, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRSIOffset

template_start loadRDXOffset
   mov rdx, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRDXOffset

template_start loadRCXOffset
   mov rcx, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRCXOffset

template_start loadRBXOffset
   mov rbx, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadRBXOffset

template_start loadR9Offset
   mov r9, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR9Offset

template_start loadR10Offset
   mov r10, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR10Offset

template_start loadR11Offset
   mov r11, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR11Offset

template_start loadR12Offset
   mov r12, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR12Offset

template_start loadR13Offset
   mov r13, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR13Offset

template_start loadR14Offset
   mov r14, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR14Offset

template_start loadR15Offset
   mov r15, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadR15Offset

template_start loadXMM0Offset
   movq xmm0, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM0Offset

template_start loadXMM1Offset
   movq xmm1, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM1Offset

template_start loadXMM2Offset
   movq xmm2, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM2Offset

template_start loadXMM3Offset
   movq xmm3, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM3Offset

template_start loadXMM4Offset
   movq xmm4, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM4Offset

template_start loadXMM5Offset
   movq xmm5, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM5Offset

template_start loadXMM6Offset
   movq xmm6, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM6Offset

template_start loadXMM7Offset
   movq xmm7, [rsp+0xff] ; Used ff as all other labels look valid and 255 will be rare.
template_end loadXMM7Offset

template_start saveRAXOffset
   mov [rsp+0xff], rax ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRAXOffset

template_start saveRSIOffset
   mov [rsp+0xff], rsi ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRSIOffset

template_start saveRDXOffset
   mov [rsp+0xff], rdx ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRDXOffset

template_start saveRCXOffset
   mov [rsp+0xff], rcx ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRCXOffset

template_start saveRBXOffset
   mov [rsp+0xff], rbx ; Used ff as all other labels look valid and 255 will be rare.
template_end saveRBXOffset

template_start saveR9Offset
   mov [rsp+0xff], r9 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR9Offset

template_start saveR10Offset
   mov [rsp+0xff], r10 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR10Offset

template_start saveR11Offset
   mov [rsp+0xff], r11 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR11Offset

template_start saveR12Offset
   mov [rsp+0xff], r12 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR12Offset

template_start saveR13Offset
   mov [rsp+0xff], r13 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR13Offset

template_start saveR14Offset
   mov [rsp+0xff], r14 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR14Offset

template_start saveR15Offset
   mov [rsp+0xff], r15 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveR15Offset

template_start saveXMM0Offset
   movq [rsp+0xff], xmm0 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM0Offset

template_start saveXMM1Offset
   movq [rsp+0xff], xmm1 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM1Offset

template_start saveXMM2Offset
   movq [rsp+0xff], xmm2 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM2Offset

template_start saveXMM3Offset
   movq [rsp+0xff], xmm3 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM3Offset

template_start saveXMM4Offset
   movq [rsp+0xff], xmm4 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM4Offset

template_start saveXMM5Offset
   movq [rsp+0xff], xmm5 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM5Offset

template_start saveXMM6Offset
   movq [rsp+0xff], xmm6 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM6Offset

template_start saveXMM7Offset
   movq [rsp+0xff], xmm7 ; Used ff as all other labels look valid and 255 will be rare.
template_end saveXMM7Offset

template_start call4ByteRel
   call 0xefbeadde ; Used for generating jumps to far labels.
template_end call4ByteRel

template_start callByteRel
   call 0xff ; Used for generating jumps to nearby labels, used ff as all other labels look valid and 255 will be rare.
template_end callByteRel

template_start jump4ByteRel
   jmp 0xefbeadde ; Used for generating jumps to far labels.
template_end jump4ByteRel

template_start jumpByteRel
   jmp short $+0x02 ; Used for generating jumps to nearby labels, shows up as eb 00 in memory (easy to see).
template_end jumpByteRel

template_start nopInstruction
   nop ; Used for alignment, one byte wide.
template_end nopInstruction

template_start movRDIImm64
   mov rdi, 0xefbeaddeefbeadde ; looks like 'deadbeef' in objdump.
template_end movRDIImm64

template_start movEDIImm32
   mov edi, 0xefbeadde ; looks like 'deadbeef' in objdump.
template_end movEDIImm32

template_start movRAXImm64
   mov rax, 0xefbeaddeefbeadde ; looks like 'deadbeef' in objdump.
template_end movRAXImm64

template_start movEAXImm32
   mov eax, 0xefbeadde ; looks like 'deadbeef' in objdump.
template_end movEAXImm32

template_start jumpRDI
   jmp rdi ; Used for jumping to absolute addresses, store in RDI then generate this.
template_end jumpRDI

template_start jumpRAX
   jmp rax ; Used for jumping to absolute addresses, store in RAX then generate this.
template_end jumpRAX

template_start paintRegister
   mov rax, qword 0x0df0adde0df0adde
template_end paintRegister

template_start paintLocal
   mov [r14+0xefbeadde], rax
template_end paintLocal

template_start moveCountAndRecompile
   mov eax, [qword 0xefbeaddeefbeadde]
template_end moveCountAndRecompile

template_start checkCountAndRecompile
   test eax,eax
   je 0xefbeadde
template_end checkCountAndRecompile

template_start loadCounter
   mov eax, [qword 0xefbeaddeefbeadde]
template_end loadCounter

template_start decrementCounter
   sub eax, 1
   mov [qword 0xefbeaddeefbeadde], eax
template_end decrementCounter

template_start incrementCounter
   inc rax
   mov [qword 0xefbeaddeefbeadde], eax
template_end incrementCounter

template_start jgCount
   test eax, eax
   jg 0xefbeadde
template_end jgCount

template_start callRetranslateArg1
   mov rax, qword 0x0df0adde0df0adde
template_end callRetranslateArg1

template_start callRetranslateArg2
   mov rsi, qword 0x0df0adde0df0adde
template_end callRetranslateArg2

template_start callRetranslate
   mov edi, 0x00080000
   call 0xefbeadde
template_end callRetranslate

template_start setCounter
   mov rax, 0x2710
   mov [qword 0xefbeaddeefbeadde], eax
template_end setCounter

template_start jmpToBody
   jmp 0xefbeadde
template_end jmpToBody
