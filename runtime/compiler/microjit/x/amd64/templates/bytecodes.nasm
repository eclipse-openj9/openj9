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
%include "microjit.nasm"

;MicroJIT Virtual Machine to X86-64 mapping
;  rsp:
;    is the stack extent for the java value stack pointer.
;  r10:
;    will hold the java value stack pointer.
;  r11:
;    stores the accumulator, or
;    stores a pointer to an object
;  r12:
;    stores any value which will act on the accumulator, or
;    stores the value to be written to an object field, or
;    stores the value read from an object field
;  r13:
;    holds addresses for absolute addressing
;  r14:
;    holds a pointer to the start of the local array
;  r15:
;    stores values loaded from memory for storing on the stack or in the local array

template_start debugBreakpoint
    int3           ; trigger hardware interrupt
template_end debugBreakpoint

; All integer loads require adding stack space of 8 bytes
; and the moving of values from a stack slot to a temp register
template_start aloadTemplatePrologue
    push_single_slot
    _64bit_local_to_rXX_PATCH r15
template_end aloadTemplatePrologue

; All integer loads require adding stack space of 8 bytes
; and the moving of values from a stack slot to a temp register
template_start iloadTemplatePrologue
    push_single_slot
    _32bit_local_to_rXX_PATCH r15
template_end iloadTemplatePrologue

; All long loads require adding 2 stack spaces of 8 bytes (16 total)
; and the moving of values from a stack slot to a temp register
template_start lloadTemplatePrologue
    push_dual_slot
    _64bit_local_to_rXX_PATCH r15
template_end lloadTemplatePrologue

template_start loadTemplate
    mov [r10], r15
template_end loadTemplate

template_start invokeStaticTemplate
    mov rdi, qword 0xefbeaddeefbeadde
template_end invokeStaticTemplate

template_start retTemplate_sub
    sub r10, byte 0xFF
template_end retTemplate_sub

template_start loadeaxReturn
    _32bit_slot_stack_from_eXX ax,0
template_end loadeaxReturn

template_start loadraxReturn
    _64bit_slot_stack_from_rXX rax,0
template_end loadraxReturn

template_start loadxmm0Return
    movd [r10], xmm0
template_end loadxmm0Return

template_start loadDxmm0Return
    movq [r10], xmm0
template_end loadDxmm0Return

template_start moveeaxForCall
    _32bit_slot_stack_to_eXX ax,0
    pop_single_slot
template_end moveeaxForCall

template_start moveesiForCall
    _32bit_slot_stack_to_eXX si,0
    pop_single_slot
template_end moveesiForCall

template_start moveedxForCall
    _32bit_slot_stack_to_eXX dx,0
    pop_single_slot
template_end moveedxForCall

template_start moveecxForCall
    _32bit_slot_stack_to_eXX cx,0
    pop_single_slot
template_end moveecxForCall

template_start moveraxRefForCall
    _64bit_slot_stack_to_rXX rax,0
    pop_single_slot
template_end moveraxRefForCall

template_start moversiRefForCall
    _64bit_slot_stack_to_rXX rsi,0
    pop_single_slot
template_end moversiRefForCall

template_start moverdxRefForCall
    _64bit_slot_stack_to_rXX rdx,0
    pop_single_slot
template_end moverdxRefForCall

template_start movercxRefForCall
    _64bit_slot_stack_to_rXX rcx,0
    pop_single_slot
template_end movercxRefForCall

template_start moveraxForCall
    _64bit_slot_stack_to_rXX rax,0
    pop_dual_slot
template_end moveraxForCall

template_start moversiForCall
    _64bit_slot_stack_to_rXX rsi,0
    pop_dual_slot
template_end moversiForCall

template_start moverdxForCall
    _64bit_slot_stack_to_rXX rdx,0
    pop_dual_slot
template_end moverdxForCall

template_start movercxForCall
    _64bit_slot_stack_to_rXX rcx,0
    pop_dual_slot
template_end movercxForCall

template_start movexmm0ForCall
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    movss xmm0, [r10]
    pop_single_slot
template_end movexmm0ForCall

template_start movexmm1ForCall
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm1, [r10]
    pop_single_slot
template_end movexmm1ForCall

template_start movexmm2ForCall
    psrldq xmm2, 8                          ; clear xmm2 by shifting right
    movss xmm2, [r10]
    pop_single_slot
template_end movexmm2ForCall

template_start movexmm3ForCall
    psrldq xmm3, 8                          ; clear xmm3 by shifting right
    movss xmm3, [r10]
    pop_single_slot
template_end movexmm3ForCall

template_start moveDxmm0ForCall
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    movsd xmm0, [r10]
    pop_dual_slot
template_end moveDxmm0ForCall

template_start moveDxmm1ForCall
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm1, [r10]
    pop_dual_slot
template_end moveDxmm1ForCall

template_start moveDxmm2ForCall
    psrldq xmm2, 8                          ; clear xmm2 by shifting right
    movsd xmm2, [r10]
    pop_dual_slot
template_end moveDxmm2ForCall

template_start moveDxmm3ForCall
    psrldq xmm3, 8                          ; clear xmm3 by shifting right
    movsd xmm3, [r10]
    pop_dual_slot
template_end moveDxmm3ForCall

template_start astoreTemplate
    _64bit_slot_stack_to_rXX r15,0
    pop_single_slot
    _64bit_local_from_rXX_PATCH r15
template_end astoreTemplate

template_start istoreTemplate
    _32bit_slot_stack_to_rXX r15,0
    pop_single_slot
    _32bit_local_from_rXX_PATCH r15
template_end istoreTemplate

template_start lstoreTemplate
    _64bit_slot_stack_to_rXX r15,0
    pop_dual_slot
    _64bit_local_from_rXX_PATCH r15
template_end lstoreTemplate

; stack before: ..., value →
; stack after:  ...
template_start popTemplate
    pop_single_slot                         ; remove the top value of the stack
template_end popTemplate

; stack before: ..., value2, value1 →
; stack after:  ...
template_start pop2Template
    pop_dual_slot                           ; remove the top two values from the stack
template_end pop2Template

; stack before: ..., value2, value1 →
; stack after:  ..., value1, value2
template_start swapTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy value1 into r12
    _64bit_slot_stack_to_rXX r11,8          ; copy value2 into r11
    _64bit_slot_stack_from_rXX r12,8        ; write value1 to the second stack slot
    _64bit_slot_stack_from_rXX r11,0        ; write value2 to the top stack slot
template_end swapTemplate

; stack before: ..., value1 →
; stack after: ..., value1, value1
template_start dupTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy the top value into r12
    push_single_slot                        ; increase the stack by 1 slot
    _64bit_slot_stack_from_rXX r12,0        ; push the duplicate on top of the stack
template_end dupTemplate

; stack before: ..., value2, value1 →
; stack after: ..., value1, value2, value1
template_start dupx1Template
    _64bit_slot_stack_to_rXX r12,0          ; copy value1 into r12
    _64bit_slot_stack_to_rXX r11,8          ; copy value2 into r11
    _64bit_slot_stack_from_rXX r12,8        ; store value1 in third stack slot
    _64bit_slot_stack_from_rXX r11,0        ; store value2 in second stack slot
    push_single_slot                        ; increase stack by 1 slot
    _64bit_slot_stack_from_rXX r12,0        ; store value1 on top stack slot
template_end dupx1Template

; stack before: ..., value3, value2, value1 →
; stack after: ..., value1, value3, value2, value1
template_start dupx2Template
    _64bit_slot_stack_to_rXX r12,0          ; copy value1 into r12
    _64bit_slot_stack_to_rXX r11,8          ; copy value2 into r11
    _64bit_slot_stack_to_rXX r15,16         ; copy value3 into r15
    _64bit_slot_stack_from_rXX r12,16       ; store value1 in fourth stack slot
    _64bit_slot_stack_from_rXX r15,8        ; store value3 in third stack slot
    _64bit_slot_stack_from_rXX r11,0        ; store value2 in second stack slot
    push_single_slot                        ; increase stack by 1 slot
    _64bit_slot_stack_from_rXX r12,0        ; store value1 in top stack slot
template_end dupx2Template

; stack before: ..., value2, value1 →
; stack after: ..., value2, value1, value2, value1
template_start dup2Template
    _64bit_slot_stack_to_rXX r12,0          ; copy value1 into r12
    _64bit_slot_stack_to_rXX r11,8          ; copy value2 into r11
    push_dual_slot                          ; increase stack by 2 slots
    _64bit_slot_stack_from_rXX r11,8        ; store value2 in second stack slot
    _64bit_slot_stack_from_rXX r12,0        ; store value1 in top stack slot
template_end dup2Template

; stack before: ..., value3, value2, value1 →
; stack after: ..., value2, value1, value3, value2, value1
template_start dup2x1Template
    _64bit_slot_stack_to_rXX r12,0          ; copy value1 into r12
    _64bit_slot_stack_to_rXX r11,8          ; copy value2 into r11
    _64bit_slot_stack_to_rXX r15,16         ; copy value3 into r15
    _64bit_slot_stack_from_rXX r11,16       ; store value2 in fifth stack slot
    _64bit_slot_stack_from_rXX r12,8        ; store value1 in fourth stack slot
    _64bit_slot_stack_from_rXX r15,0        ; store value3 in third stack slot
    push_dual_slot                          ; increase stack by 2 slots
    _64bit_slot_stack_from_rXX r11,8        ; store value2 in second stack slot
    _64bit_slot_stack_from_rXX r12,0        ; store value in top stack slot
template_end dup2x1Template

; stack before: ..., value4, value3, value2, value1 →
; stack after: ..., value2, value1, value4, value3, value2, value1
template_start dup2x2Template
    _64bit_slot_stack_to_rXX r11,0          ; mov value1 into r11
    _64bit_slot_stack_to_rXX r12,8          ; mov value2 into r12
    _64bit_slot_stack_to_rXX r15,16         ; mov value3 into r15
    _64bit_slot_stack_from_rXX r11,16       ; mov value1 into old value3 slot
    _64bit_slot_stack_from_rXX r15,0        ; mov value3 into old value1 slot
    _64bit_slot_stack_to_rXX r15,24         ; mov value4 into r15
    _64bit_slot_stack_from_rXX r15,8        ; mov value4 into old value2 slot
    _64bit_slot_stack_from_rXX r12,24       ; mov value2 into old value4 slot
    push_single_slot                        ; increase the stack
    _64bit_slot_stack_from_rXX r12,0        ; mov value2 into second slot
    push_single_slot                        ; increase the stack
    _64bit_slot_stack_from_rXX r11,0        ; mov value1 into top stack slot
template_end dup2x2Template

template_start getFieldTemplatePrologue
    _64bit_slot_stack_to_rXX r11,0          ; get the objectref from the stack
    lea r13, [r11 + 0xefbeadde]             ; load the effective address of the object field
template_end getFieldTemplatePrologue

template_start intGetFieldTemplate
    mov r12d, dword [r13]                   ; load only 32-bits
    _32bit_slot_stack_from_rXX r12,0        ; put it on the stack
template_end intGetFieldTemplate

template_start addrGetFieldTemplatePrologue
    mov r12d, dword [r13]                   ; load only 32-bits
template_end addrGetFieldTemplatePrologue

template_start decompressReferenceTemplate
    shl r12, 0xff                           ; Reference from field is compressed, decompress by shift amount
template_end decompressReferenceTemplate

template_start decompressReference1Template
    shl r12, 0x01                           ; Reference from field is compressed, decompress by shift amount
template_end decompressReference1Template

template_start addrGetFieldTemplate
    _64bit_slot_stack_from_rXX r12,0        ; put it on the stack
template_end addrGetFieldTemplate

template_start longGetFieldTemplate
    push_single_slot                        ; increase the stack pointer to make room for extra slot
    mov r12, [r13]                          ; load the value
    _64bit_slot_stack_from_rXX r12,0        ; put it on the stack
template_end longGetFieldTemplate

template_start floatGetFieldTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    movss xmm0, dword [r13]                 ; load the value
    movss [r10], xmm0                       ; put it on the stack
template_end floatGetFieldTemplate

template_start doubleGetFieldTemplate
    push_single_slot                        ; increase the stack pointer to make room for extra slot
    movsd xmm0, [r13]                       ; load the value
    movsd [r10], xmm0                       ; put it on the stack
template_end doubleGetFieldTemplate

template_start intPutFieldTemplatePrologue
    _32bit_slot_stack_to_rXX r11,0          ; copy value into r11
    pop_single_slot                         ; reduce stack pointer
    _64bit_slot_stack_to_rXX r12,0          ; get the object reference
    pop_single_slot                         ; reduce the stack pointer
    lea r13, [r12 + 0xefbeadde]             ; load the effective address of the object field
template_end intPutFieldTemplatePrologue

template_start addrPutFieldTemplatePrologue
    _64bit_slot_stack_to_rXX r11,0          ; copy value into r11
    pop_single_slot                         ; reduce stack pointer
    _64bit_slot_stack_to_rXX r12,0          ; get the object reference
    pop_single_slot                         ; reduce the stack pointer
    lea r13, [r12 + 0xefbeadde]             ; load the effective address of the object field
template_end addrPutFieldTemplatePrologue

template_start longPutFieldTemplatePrologue
    _64bit_slot_stack_to_rXX r11,0          ; copy value into r11
    pop_dual_slot                           ; reduce stack pointer
    _64bit_slot_stack_to_rXX r12,0          ; get the object reference
    pop_single_slot                         ; reduce the stack pointer
    lea r13, [r12 + 0xefbeadde]             ; load the effective address of the object field
template_end longPutFieldTemplatePrologue

template_start floatPutFieldTemplatePrologue
    movss xmm0, [r10]                       ; copy value into r11
    pop_single_slot                         ; reduce stack pointer
    _64bit_slot_stack_to_rXX r12,0          ; get the object reference
    pop_single_slot                         ; reduce the stack pointer
    lea r13, [r12 + 0xefbeadde]             ; load the effective address of the object field
template_end floatPutFieldTemplatePrologue

template_start doublePutFieldTemplatePrologue
    movsd xmm0, [r10]                       ; copy value into r11
    pop_dual_slot                           ; reduce stack pointer
    _64bit_slot_stack_to_rXX r12,0          ; get the object reference
    pop_single_slot                         ; reduce the stack pointer
    lea r13, [r12 + 0xefbeadde]             ; load the effective address of the object field
template_end doublePutFieldTemplatePrologue

template_start intPutFieldTemplate
    mov [r13], r11d                         ; Move value to memory
template_end intPutFieldTemplate

template_start compressReferenceTemplate
    shr r11, 0xff
template_end compressReferenceTemplate

template_start compressReference1Template
    shr r11, 0x01
template_end compressReference1Template

template_start longPutFieldTemplate
    mov [r13], r11                          ; Move value to memory
template_end longPutFieldTemplate

template_start floatPutFieldTemplate
    movss [r13], xmm0                       ; Move value to memory
template_end floatPutFieldTemplate

template_start doublePutFieldTemplate
    movsd [r13], xmm0                       ; Move value to memory
template_end doublePutFieldTemplate

template_start staticTemplatePrologue
    lea r13, [0xefbeadde]                   ; load the absolute address of a static value
template_end staticTemplatePrologue

template_start intGetStaticTemplate
    push_single_slot                        ; allocate a stack slot
    mov r11d, dword [r13]                   ; load 32-bit value into r11
    _32bit_slot_stack_from_rXX r11,0        ; move it onto the stack
template_end intGetStaticTemplate

template_start addrGetStaticTemplatePrologue
    push_single_slot                        ; allocate a stack slot
    mov r11, qword [r13]                    ; load 32-bit value into r11
template_end addrGetStaticTemplatePrologue

template_start addrGetStaticTemplate
    _64bit_slot_stack_from_rXX r11,0        ; move it onto the stack
template_end addrGetStaticTemplate

template_start longGetStaticTemplate
    push_dual_slot                          ; allocate a stack slot
    mov r11, [r13]                          ; load 64-bit value into r11
    _64bit_slot_stack_from_rXX r11,0        ; move it onto the stack
template_end longGetStaticTemplate

template_start floatGetStaticTemplate
    push_single_slot                        ; allocate a stack slot
    movss xmm0, [r13]                       ; load value into r11
    movss [r10], xmm0                       ; move it onto the stack
template_end floatGetStaticTemplate

template_start doubleGetStaticTemplate
    push_dual_slot                          ; allocate a stack slot
    movsd xmm0, [r13]                       ; load value into r11
    movsd [r10], xmm0                       ; move it onto the stack
template_end doubleGetStaticTemplate

template_start intPutStaticTemplate
    _32bit_slot_stack_to_rXX r11,0          ; Move stack value into r11
    pop_single_slot                         ; Pop the value off the stack
    mov dword [r13], r11d                   ; Move it to memory
template_end intPutStaticTemplate

template_start addrPutStaticTemplatePrologue
    _64bit_slot_stack_to_rXX r11,0          ; Move stack value into r11
    pop_single_slot                         ; Pop the value off the stack
template_end addrPutStaticTemplatePrologue

template_start addrPutStaticTemplate
    mov qword [r13], r11                    ; Move it to memory
template_end addrPutStaticTemplate

template_start longPutStaticTemplate
    _64bit_slot_stack_to_rXX r11,0          ; Move stack value into r11
    pop_dual_slot                           ; Pop the value off the stack
    mov [r13], r11                          ; Move it to memory
template_end longPutStaticTemplate

template_start floatPutStaticTemplate
    movss xmm0, [r10]                       ; Move stack value into r11
    pop_single_slot                         ; Pop the value off the stack
    movss [r13], xmm0                       ; Move it to memory
template_end floatPutStaticTemplate

template_start doublePutStaticTemplate
    movsd xmm0, [r10]                       ; Move stack value into r11
    pop_dual_slot                           ; Pop the value off the stack
    movsd [r13], xmm0                       ; Move it to memory
template_end doublePutStaticTemplate

template_start iAddTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; which means reducing the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r12,0          ; copy second value to the value register
    add r11d, r12d                          ; add the value to the accumulator
    _32bit_slot_stack_from_rXX r11,0        ; write the accumulator over the second arg
template_end iAddTemplate

template_start iSubTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; copy second value to the accumulator register
    sub r11d, r12d                          ; subtract the value from the accumulator
    _32bit_slot_stack_from_rXX r11,0        ; write the accumulator over the second arg
template_end iSubTemplate

template_start iMulTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; which means reducing the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r12,0          ; copy second value to the value register
    imul r11d, r12d                         ; multiply accumulator by the value to the accumulator
    _32bit_slot_stack_from_rXX r11,0        ; write the accumulator over the second arg
template_end iMulTemplate

template_start iDivTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the value register (divisor)
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_eXX ax,0           ; copy second value (dividend) to eax
    cdq                                     ; extend sign bit from eax to edx
    idiv r12d                               ; divide edx:eax by lower 32 bits of r12 value
    _32bit_slot_stack_from_eXX ax,0         ; store the quotient in accumulator
template_end iDivTemplate

template_start iRemTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the value register (divisor)
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_eXX ax,0           ; copy second value (dividend) to eax
    cdq                                     ; extend sign bit from eax to edx
    idiv r12d                               ; divide edx:eax by lower 32 bits of r12 value
    _32bit_slot_stack_from_eXX dx,0         ; store the remainder in accumulator
template_end iRemTemplate

template_start iNegTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy the top value from the stack into r12
    neg r12d                                ; two's complement negation of r12
    _32bit_slot_stack_from_rXX r12,0        ; store the result back on the stack
template_end iNegTemplate

template_start iShlTemplate
    mov cl, [r10]                           ; copy top value of stack in the cl register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_eXX ax,0           ; copy second value to eax
    sal eax, cl                             ; arithmetic left shift eax by cl bits
    _32bit_slot_stack_from_eXX ax,0         ; store the result back on the stack
template_end iShlTemplate

template_start iShrTemplate
    mov cl, [r10]                           ; copy top value of stack in the cl register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_eXX ax,0           ; copy second value to eax
    sar eax, cl                             ; arithmetic right shift eax by cl bits
    _32bit_slot_stack_from_eXX ax,0         ; store the result back on the stack
template_end iShrTemplate

template_start iUshrTemplate
    mov cl, [r10]                           ; copy top value of stack in the cl register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_eXX ax,0           ; copy second value to eax
    shr eax, cl                             ; logical right shift eax by cl bits
    _32bit_slot_stack_from_eXX ax,0         ; store the result back on the stack
template_end iUshrTemplate

template_start iAndTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the accumulator (first operand)
    pop_single_slot                         ; reduce the stack size by 1 slot
    _32bit_slot_stack_to_rXX r11,0          ; copy the second value into a register (second operand)
    and r11d, r12d                          ; perform the bitwise and operation
    _32bit_slot_stack_from_rXX r11,0        ; store the result back on the java stack
template_end iAndTemplate

template_start iOrTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the accumulator (first operand)
    pop_single_slot                         ; reduce the stack size by 1 slot
    _32bit_slot_stack_to_rXX r11,0          ; copy the second value into a register (second operand)
    or r11d, r12d                           ; perform the bitwise or operation
    _32bit_slot_stack_from_rXX r11,0        ; store the result back on the java stack
template_end iOrTemplate

template_start iXorTemplate
    _32bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the accumulator (first operand)
    pop_single_slot                         ; reduce the stack size by 1 slot
    _32bit_slot_stack_to_rXX r11,0          ; copy the second value into a register (second operand)
    xor r11d, r12d                          ; perform the bitwise xor operation
    _32bit_slot_stack_from_rXX r11,0        ; store the result back on the java stack
template_end iXorTemplate

template_start i2lTemplate
    movsxd r12, dword [r10]                 ; takes the bottom 32 from the stack bits and sign extends
    push_single_slot                        ; add slot for long (1 slot -> 2 slot)
    _64bit_slot_stack_from_rXX r12,0        ; push the result back on the stack
template_end i2lTemplate

template_start l2iTemplate
    movsxd r12, [r10]                       ; takes the bottom 32 from the stack bits and sign extends
    pop_single_slot                         ; remove extra slot for long (2 slot -> 1 slot)
    _32bit_slot_stack_from_rXX r12,0        ; push the result back on the stack
template_end l2iTemplate

template_start i2bTemplate
    movsx r12, byte [r10]                   ; takes the top byte from the stack and sign extends
    _32bit_slot_stack_from_rXX r12,0        ; push the result back on the stack
template_end i2bTemplate

template_start i2sTemplate
    movsx r12, word [r10]                   ; takes the top word from the stack and sign extends
    _32bit_slot_stack_from_rXX r12,0        ; push the result back on the stack
template_end i2sTemplate

template_start i2cTemplate
    movzx r12, word [r10]                   ; takes the top word from the stack and zero extends
    _32bit_slot_stack_from_rXX r12,0        ; push the result back on the stack
template_end i2cTemplate

template_start i2dTemplate
    cvtsi2sd xmm0, dword [r10]              ; convert signed 32 bit integer to double-presision fp
    push_single_slot                        ; make room for the double
    movsd [r10], xmm0                       ; push on top of stack
template_end i2dTemplate

template_start l2dTemplate
    cvtsi2sd xmm0, qword [r10]              ; convert signed 64 bit integer to double-presision fp
    movsd [r10], xmm0                       ; push on top of stack
template_end l2dTemplate

template_start d2iTemplate
    movsd xmm0, [r10]                       ; load value from top of stack
    mov r11, 2147483647                     ; store max int in r11
    mov r12, -2147483648                    ; store min int in r12
    mov rcx, 0                              ; store 0 in rcx
    cvtsi2sd xmm1, r11d                     ; convert max int to double
    cvtsi2sd xmm2, r12d                     ; convert min int to double
    cvttsd2si rax, xmm0                     ; convert double to int with truncation
    ucomisd xmm0, xmm1                      ; compare value with max int
    cmovae rax, r11                         ; if value is above or equal to max int, store max int in rax
    ucomisd xmm0, xmm2                      ; compare value with min int
    cmovb rax, r12                          ; if value is below min int, store min int in rax
    cmovp rax, rcx                          ; if value is NaN, store 0 in rax
    pop_single_slot                         ; remove extra slot for double (2 slot -> 1 slot)
    _64bit_slot_stack_from_rXX rax,0        ; store result back on stack
template_end d2iTemplate

template_start d2lTemplate
    movsd xmm0, [r10]                       ; load value from top of stack
    mov r11, 9223372036854775807            ; store max long in r11
    mov r12, -9223372036854775808           ; store min long in r12
    mov rcx, 0                              ; store 0 in rcx
    cvtsi2sd xmm1, r11                      ; convert max long to double
    cvtsi2sd xmm2, r12                      ; convert min long to double
    cvttsd2si rax, xmm0                     ; convert double to long with truncation
    ucomisd xmm0, xmm1                      ; compare value with max int
    cmovae rax, r11                         ; if value is above or equal to max long, store max int in rax
    ucomisd xmm0, xmm2                      ; compare value with min int
    cmovb rax, r12                          ; if value is below min int, store min int in rax
    cmovp rax, rcx                          ; if value is NaN, store 0 in rax
    _64bit_slot_stack_from_rXX rax,0        ; store back on stack
template_end d2lTemplate

template_start iconstm1Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], -1                     ; push -1 to the java stack
template_end iconstm1Template

template_start iconst0Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], 0                      ; push 0 to the java stack
template_end iconst0Template

template_start iconst1Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], 1                      ; push 1 to the java stack
template_end iconst1Template

template_start iconst2Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], 2                      ; push 2 to the java stack
template_end iconst2Template

template_start iconst3Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], 3                      ; push 3 to the java stack
template_end iconst3Template

template_start iconst4Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], 4                      ; push 4 to the java stack
template_end iconst4Template

template_start iconst5Template
    push_single_slot                        ; increase the java stack size by 1 slot (8 bytes)
    mov qword [r10], 5                      ; push 5 to the java stack
template_end iconst5Template

template_start bipushTemplate
    push_single_slot                        ; increase the stack by 1 slot
    mov qword [r10], 0xFF                   ; push immediate value to stack
template_end bipushTemplate

template_start sipushTemplatePrologue
    mov r11w, 0xFF                          ; put immediate value in accumulator for sign extension
template_end sipushTemplatePrologue

template_start sipushTemplate
    movsx r11, r11w                         ; sign extend the contents of r11w through the rest of r11
    push_single_slot                        ; increase the stack by 1 slot
    _32bit_slot_stack_from_rXX r11,0        ; push immediate value to stack
template_end sipushTemplate

template_start iIncTemplate_01_load
    _32bit_local_to_rXX_PATCH r11
template_end iIncTemplate_01_load

template_start iIncTemplate_02_add
    add r11, byte 0xFF
template_end iIncTemplate_02_add

template_start iIncTemplate_03_store
    _32bit_local_from_rXX_PATCH r11
template_end iIncTemplate_03_store

template_start lAddTemplate
    _64bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r12,0          ; copy second value to the value register
    add r11, r12                            ; add the value to the accumulator
    _64bit_slot_stack_from_rXX r11,0        ; write the accumulator over the second arg
template_end lAddTemplate

template_start lSubTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the value register
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r11,0          ; copy second value to the accumulator register
    sub r11, r12                            ; subtract the value from the accumulator
    _64bit_slot_stack_from_rXX r11,0        ; write the accumulator over the second arg
template_end lSubTemplate

template_start lMulTemplate
    _64bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r12,0          ; copy second value to the value register
    imul r11, r12                           ; multiply accumulator by the value to the accumulator
    _64bit_slot_stack_from_rXX r11,0        ; write the accumulator over the second arg
template_end lMulTemplate

template_start lDivTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the value register (divisor)
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX rax,0          ; copy second value (dividend) to rax
    cqo                                     ; extend sign bit from rax to rdx
    idiv r12                                ; divide rdx:rax by r12 value
    _64bit_slot_stack_from_rXX rax,0        ; store the quotient in accumulator
template_end lDivTemplate

template_start lRemTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the value register (divisor)
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX rax,0          ; copy second value (dividend) to rax
    cqo                                     ; extend sign bit from rax to rdx
    idiv r12                                ; divide rdx:rax by r12 value
    _64bit_slot_stack_from_rXX rdx,0        ; store the remainder in accumulator
template_end lRemTemplate

template_start lNegTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy the top value from the stack into r12
    neg r12                                 ; two's complement negation of r12
    _64bit_slot_stack_from_rXX r12,0        ; store the result back on the stack
template_end lNegTemplate

template_start lShlTemplate
    mov cl, [r10]                           ; copy top value of stack in the cl register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _64bit_slot_stack_to_rXX rax,0          ; copy second value to rax
    sal rax, cl                             ; arithmetic left shift rax by cl bits
    _64bit_slot_stack_from_rXX rax,0        ; store the result back on the stack
template_end lShlTemplate

template_start lShrTemplate
    mov cl, [r10]                           ; copy top value of stack in the cl register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _64bit_slot_stack_to_rXX rax,0          ; copy second value to rax
    sar rax, cl                             ; arithmetic right shift rax by cl bits
    _64bit_slot_stack_from_rXX rax,0        ; store the result back on the stack
template_end lShrTemplate

template_start lUshrTemplate
    mov cl, [r10]                           ; copy top value of stack in the cl register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _64bit_slot_stack_to_rXX rax,0          ; copy second value to rax
    shr rax, cl                             ; logical right shift rax by cl bits
    _64bit_slot_stack_from_rXX rax,0        ; store the result back on the stack
template_end lUshrTemplate

template_start lAndTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the accumulator (first operand)
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r11,0          ; copy the second value into a register (second operand)
    and r11, r12                            ; perform the bitwise and operation
    _64bit_slot_stack_from_rXX r11,0        ; store the result back on the java stack
template_end lAndTemplate

template_start lOrTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the accumulator (first operand)
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r11,0          ; copy the second value into a register (second operand)
    or r11 , r12                            ; perform the bitwise or operation
    _64bit_slot_stack_from_rXX r11,0        ; store the result back on the java stack
template_end lOrTemplate

template_start lXorTemplate
    _64bit_slot_stack_to_rXX r12,0          ; copy top value of stack in the accumulator (first operand)
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r11,0          ; copy the second value into a register (second operand)
    xor r11 , r12                           ; perform the bitwise xor operation
    _64bit_slot_stack_from_rXX r11,0        ; store the result back on the java stack
template_end lXorTemplate

template_start lconst0Template
    push_dual_slot                          ; increase the stack size by 2 slots (16 bytes)
    mov qword [r10], 0                      ; push 0 to the java stack
template_end lconst0Template

template_start lconst1Template
    push_dual_slot                          ; increase the stack size by 2 slots (16 bytes)
    mov qword [r10], 1                      ; push 1 to the java stack
template_end lconst1Template

template_start fAddTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_single_slot                         ; reduce the stack size by 1 slot
    movss xmm0, [r10]                       ; copy the second value in xmm0
    addss xmm0, xmm1                        ; add the values in xmm registers and store in xmm0
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end fAddTemplate

template_start fSubTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_single_slot                         ; reduce the stack size by 1 slot
    movss xmm0, [r10]                       ; copy the second value in xmm0
    subss xmm0, xmm1                        ; subtract the value of xmm1 from xmm0 and store in xmm0
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end fSubTemplate

template_start fMulTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_single_slot                         ; reduce the stack size by 1 slot
    movss xmm0, [r10]                       ; copy the second value in xmm0
    mulss xmm0, xmm1                        ; multiply the values in xmm registers and store in xmm0
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end fMulTemplate

template_start fDivTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_single_slot                         ; reduce the stack size by 1 slot
    movss xmm0, [r10]                       ; copy the second value in xmm0
    divss xmm0, xmm1                        ; divide the value of xmm0 by xmm1 and store in xmm0
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end fDivTemplate

template_start fRemTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm0, [r10]                       ; copy top value of stack in xmm0
    pop_single_slot                         ; reduce the stack size by 1 slot
    movss xmm1, [r10]                       ; copy the second value in xmm1
    call 0xefbeadde                         ; for function call
template_end fRemTemplate

template_start fNegTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movss xmm0, [r10]                       ; copy top value of stack in xmm0
    mov eax, 0x80000000                     ; loading mask immediate value to xmm1 through eax
    movd xmm1, eax                          ; as it can't be directly loaded to xmm registers
    xorps xmm0, xmm1                        ; XOR xmm0 and xmm1 mask bits to negate xmm0
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end fNegTemplate

template_start fconst0Template
    push_single_slot                        ; increase the java stack size by 1 slot
    fldz                                    ; push 0 onto the fpu register stack
    fbstp [r10]                             ; stores st0 on the java stack
template_end fconst0Template

template_start fconst1Template
    push_single_slot                        ; increase the java stack size by 1 slot
    fld1                                    ; push 1 onto the fpu register stack
    fbstp [r10]                             ; stores st0 on the java stack
template_end fconst1Template

template_start fconst2Template
    push_single_slot                        ; increase the java stack size by 1 slot
    fld1                                    ; push 1 on the fpu register stack
    fld1                                    ; push another 1 on the fpu register stack
    fadd st0, st1                           ; add the top two values together and store them back on the top of the stack
    fbstp [r10]                             ; stores st0 on the java stack
template_end fconst2Template

template_start dconst0Template
    push_dual_slot                          ; increase the java stack size by 2 slots
    fldz                                    ; push 0 onto the fpu register stack
    fbstp [r10]                             ; stores st0 on the java stack
template_end dconst0Template

template_start dconst1Template
    push_dual_slot                          ; increase the java stack size by 2 slots
    fld1                                    ; push 1 onto the fpu register stack
    fbstp [r10]                             ; stores st0 on the java stack
template_end dconst1Template

template_start dAddTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    movsd xmm0, [r10]                       ; copy the second value in xmm0
    addsd xmm0, xmm1                        ; add the values in xmm registers and store in xmm0
    movsd [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end dAddTemplate

template_start dSubTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    movsd xmm0, [r10]                       ; copy the second value in xmm0
    subsd xmm0, xmm1                        ; subtract the value of xmm1 from xmm0 and store in xmm0
    movsd [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end dSubTemplate

template_start dMulTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    movsd xmm0, [r10]                       ; copy the second value in xmm0
    mulsd xmm0, xmm1                        ; multiply the values in xmm registers and store in xmm0
    movsd [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end dMulTemplate

template_start dDivTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm1, [r10]                       ; copy top value of stack in xmm1
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    movsd xmm0, [r10]                       ; copy the second value in xmm0
    divsd xmm0, xmm1                        ; divide the value of xmm0 by xmm1 and store in xmm0
    movsd [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end dDivTemplate

template_start dRemTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm0, [r10]                       ; copy top value of stack in xmm0
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    movsd xmm1, [r10]                       ; copy the second value in xmm1
    call 0xefbeadde                         ; for function call
template_end dRemTemplate

template_start dNegTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    movsd xmm0, [r10]                       ; copy top value of stack in xmm0
    mov rax, 0x8000000000000000             ; loading mask immediate value to xmm1 through rax
    movq xmm1, rax                          ; as it can't be directly loaded to xmm registers
    xorpd xmm0, xmm1                        ; XOR xmm0 and xmm1 mask bits to negate xmm0
    movsd [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end dNegTemplate

template_start i2fTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    cvtsi2ss xmm0, [r10]                    ; convert integer to float and store in xmm0
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end i2fTemplate

template_start f2iTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    psrldq xmm2, 8                          ; clear xmm2 by shifting right
    movss xmm0, [r10]                       ; copy top value of stack in xmm0
    mov r11d, 2147483647                    ; store max int in r11
    mov r12d, -2147483648                   ; store min int in r12
    mov ecx, 0                              ; store 0 in rcx
    cvtsi2ss xmm1, r11d                     ; convert max int to float
    cvtsi2ss xmm2, r12d                     ; convert min int to float
    cvttss2si eax, xmm0                     ; convert float to int with truncation
    ucomiss xmm0, xmm1                      ; compare value with max int
    cmovae eax, r11d                        ; if value is above or equal to max int, store max int in rax
    ucomiss xmm0, xmm2                      ; compare value with min int
    cmovb eax, r12d                         ; if value is below min int, store min int in rax
    cmovp eax, ecx                          ; if value is NaN, store 0 in rax
    _32bit_slot_stack_from_eXX ax,0         ; store result back on stack
template_end f2iTemplate

template_start l2fTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    cvtsi2ss xmm0, qword [r10]              ; convert long to float and store in xmm0
    pop_single_slot                         ; remove extra slot (2 slot -> 1 slot)
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end l2fTemplate

template_start f2lTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    psrldq xmm1, 8                          ; clear xmm1 by shifting right
    psrldq xmm2, 8                          ; clear xmm2 by shifting right
    movss xmm0, [r10]                       ; copy top value of stack in xmm0
    mov r11, 9223372036854775807            ; store max long in r11
    mov r12, -9223372036854775808           ; store min long in r12
    mov rcx, 0                              ; store 0 in rcx
    cvtsi2ss xmm1, r11                      ; convert max long to float
    cvtsi2ss xmm2, r12                      ; convert min long to float
    cvttss2si rax, xmm0                     ; convert float to long with truncation
    ucomiss xmm0, xmm1                      ; compare value with max long
    cmovae rax, r11                         ; if value is above or equal to max long, store max long in rax
    ucomiss xmm0, xmm2                      ; compare value with min int
    cmovb rax, r12                          ; if value is below min long, store min long in rax
    cmovp rax, rcx                          ; if value is NaN, store 0 in rax
    push_single_slot                        ; add extra slot for long (1 slot -> 2 slot)
    _64bit_slot_stack_from_rXX rax,0        ; store result back on stack
template_end f2lTemplate

template_start d2fTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    movsd xmm0, [r10]                       ; copy top value of stack in xmm0
    cvtsd2ss xmm0, xmm0                     ; convert double to float and store in xmm0
    pop_single_slot                         ; remove extra slot from double (2 slot -> 1 slot)
    movss [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end d2fTemplate

template_start f2dTemplate
    psrldq xmm0, 8                          ; clear xmm0 by shifting right
    movss xmm0, [r10]                       ; copy top value of stack in xmm0
    cvtss2sd xmm0, xmm0                     ; convert float to double and store in xmm0
    push_single_slot                        ; add slot to stack for double (1 slot -> 2 slot)
    movsd [r10], xmm0                       ; store the result in xmm0 on the java stack
template_end f2dTemplate

template_start eaxReturnTemplate
  xor rax, rax    ; clear rax
  _32bit_slot_stack_to_eXX ax,0             ; move the stack top into eax register
template_end eaxReturnTemplate

template_start raxReturnTemplate
  _64bit_slot_stack_to_rXX rax,0            ; move the stack top into rax register
template_end raxReturnTemplate

template_start xmm0ReturnTemplate
  psrldq xmm0, 8                            ; clear xmm0 by shifting right
  movd xmm0, [r10]                          ; move the stack top into xmm0 register
template_end xmm0ReturnTemplate

template_start retTemplate_add
    add r10, byte 0xFF                      ; decrease stack size
template_end retTemplate_add

template_start vReturnTemplate
    ret                                     ; return from the JITed method
template_end vReturnTemplate

; Stack before: ..., value1, value2 ->
; Stack after:  ..., result
; if value1 > value2, result = 1
; if value1 == value 2, result = 0
; if value1 < value 2, result = -1
template_start lcmpTemplate
    _64bit_slot_stack_to_rXX r11,0          ; grab value2 from the stack
    pop_dual_slot                           ; reduce the stack size by 2 slots (16 bytes)
    _64bit_slot_stack_to_rXX r12,0          ; grab value1 from the stack
    pop_single_slot                         ; reduce long slot to int slot
    xor ecx, ecx                            ; clear ecx (store 0)
    mov dword edx, -1                       ; store -1 in edx
    mov dword ebx, 1                        ; store 1 in ebx
    cmp r12, r11                            ; compare the two values
    cmovg eax, ebx                          ; mov 1 into eax if ZF = 0 and CF = 0
    cmovl eax, edx                          ; mov -1 into eax if CF = 1
    cmovz eax, ecx                          ; mov 0 into eax if ZF = 1
    _32bit_slot_stack_from_eXX ax,0         ; store result on top of stack
template_end lcmpTemplate

; Stack before: ..., value1, value2 ->
; Stack after:  ..., result
; if value1 > value2, result = 1            Flags: ZF,PF,CF --> 000
; else if value1 == value 2, result = 0     Flags: ZF,PF,CF --> 100
; else if value1 < value 2, result = -1     Flags: ZF,PF,CF --> 001
; else, one of value1 or value2 must be NaN -> result = -1      Flags: ZF,PF,CF --> 111
template_start fcmplTemplate
    movss xmm0, dword [r10]                 ; copy value2 into xmm0
    pop_single_slot                         ; decrease the stack by 1 slot
    movss xmm1, dword [r10]                 ; copy value1 into xmm1
    xor r11d, r11d                          ; clear r11 (store 0)
    mov dword eax, 1                        ; store 1 in rax
    mov dword ecx, -1                       ; store -1 in rcx
    comiss xmm1, xmm0                       ; compare the values
    cmove r12d, r11d                        ; mov 0 into r12 if ZF = 1
    cmova r12d, eax                         ; mov 1 into r12 if ZF = 0 and CF = 0
    cmovb r12d, ecx                         ; mov -1 into r12 if CF = 1
    cmovp r12d, ecx                         ; mov -1 into r12 if PF = 1
    _32bit_slot_stack_from_rXX r12,0        ; store result on top of stack
template_end fcmplTemplate

; Stack before: ..., value1, value2 ->
; Stack after:  ..., result
; if value1 > value2, result = 1            Flags: ZF,PF,CF --> 000
; else if value1 == value 2, result = 0     Flags: ZF,PF,CF --> 100
; else if value1 < value 2, result = -1     Flags: ZF,PF,CF --> 001
; else, one of value1 or value2 must be NaN -> result = 1       Flags: ZF,PF,CF --> 111
template_start fcmpgTemplate
    movss xmm0, dword [r10]                 ; copy value2 into xmm0
    pop_single_slot                         ; decrease the stack by 1 slot
    movss xmm1, dword [r10]                 ; copy value1 into xmm1
    xor r11d, r11d                          ; clear r11 (store 0)
    mov dword eax, 1                        ; store 1 in rax
    mov dword ecx, -1                       ; store -1 in rcx
    comiss xmm1, xmm0                       ; compare the values
    cmove r12d, r11d                        ; mov 0 into r12 if ZF = 1
    cmovb r12d, ecx                         ; mov -1 into r12 if CF = 1
    cmova r12d, eax                         ; mov 1 into r12 if ZF = 0 and CF = 0
    cmovp r12d, eax                         ; mov 1 into r12 if PF = 1
    _32bit_slot_stack_from_rXX r12,0        ; store result on top of stack
template_end fcmpgTemplate

; Stack before: ..., value1, value2 ->
; Stack after:  ..., result
; if value1 > value2, result = 1            Flags: ZF,PF,CF --> 000
; else if value1 == value 2, result = 0     Flags: ZF,PF,CF --> 100
; else if value1 < value 2, result = -1     Flags: ZF,PF,CF --> 001
; else, one of value1 or value2 must be NaN -> result = -1      Flags: ZF,PF,CF --> 111
template_start dcmplTemplate
    movsd xmm0, qword [r10]                 ; copy value2 into xmm0
    pop_dual_slot                           ; decrease the stack by 2 slots
    movsd xmm1, qword [r10]                 ; copy value1 into xmm1
    pop_single_slot                         ; decrease the stack by 1 slot
    xor r11d, r11d                          ; clear r11 (store 0)
    mov dword eax, 1                        ; store 1 in rax
    mov dword ecx, -1                       ; store -1 in rcx
    comisd xmm1, xmm0                       ; compare the values
    cmove r12d, r11d                        ; mov 0 into r12 if ZF = 1
    cmova r12d, eax                         ; mov 1 into r12 if ZF = 0 and CF = 0
    cmovb r12d, ecx                         ; mov -1 into r12 if CF = 1
    cmovp r12d, ecx                         ; mov -1 into r12 if PF = 1
    _32bit_slot_stack_from_rXX r12,0        ; store result on top of stack
template_end dcmplTemplate

; Stack before: ..., value1, value2 ->
; Stack after:  ..., result
; if value1 > value2, result = 1            Flags: ZF,PF,CF --> 000
; else if value1 == value 2, result = 0     Flags: ZF,PF,CF --> 100
; else if value1 < value 2, result = -1     Flags: ZF,PF,CF --> 001
; else, one of value1 or value2 must be NaN -> result = 1       Flags: ZF,PF,CF --> 111
template_start dcmpgTemplate
    movsd xmm0, qword [r10]                 ; copy value2 into xmm0
    pop_dual_slot                           ; decrease the stack by 1 slot
    movsd xmm1, qword [r10]                 ; copy value1 into xmm1
    pop_single_slot                         ; decrease the stack by 1 slot
    xor r11d, r11d                          ; clear r11 (store 0)
    mov dword eax, 1                        ; store 1 in rax
    mov dword ecx, -1                       ; store -1 in rcx
    comisd xmm1, xmm0                       ; compare the values
    cmove r12d, r11d                        ; mov 0 into r12 if ZF = 1
    cmovb r12d, ecx                         ; mov -1 into r12 if CF = 1
    cmova r12d, eax                         ; mov 1 into r12 if ZF = 0 and CF = 0
    cmovp r12d, eax                         ; mov 1 into r12 if PF = 1
    _32bit_slot_stack_from_rXX r12,0        ; store result on top of stack
template_end dcmpgTemplate

; ifne succeeds if and only if value ≠ 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifneTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    test r11d, r11d                         ; r11 - r11
    jnz 0xefbeadde                          ; Used for generating jump not equal to far labels
template_end ifneTemplate

; ifeq succeeds if and only if value = 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifeqTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    test r11d, r11d                         ; r11 - r11
    jz 0xefbeadde                           ; Used for generating jump to far labels if ZF = 1
template_end ifeqTemplate

; iflt succeeds if and only if value < 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifltTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    test r11d, r11d                         ; r11 - r11
    jl 0xefbeadde                           ; Used for generating jump to far labels if SF <> OF
template_end ifltTemplate

; ifge succeeds if and only if value >= 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifgeTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    test r11d, r11d                         ; r11 - r11
    jge 0xefbeadde                          ; Used for generating jump to far labels if SF = OF
template_end ifgeTemplate

; ifgt succeeds if and only if value > 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifgtTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    test r11d, r11d                         ; r11 - r11
    jg 0xefbeadde                           ; Used for generating jump to far labels if ZF = 0 and SF = OF
template_end ifgtTemplate

; ifle succeeds if and only if value <= 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifleTemplate
    _32bit_slot_stack_to_rXX r11,0          ; pop first value off java stack into the accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    test r11d, r11d                         ; r11 - r11
    jle 0xefbeadde                          ; Used for generating jump to far labels if ZF = 1 or SF <> OF
template_end ifleTemplate

; if_icmpeq succeeds if and only if value1 = value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ificmpeqTemplate
    _32bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11d, r12d                          ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    je 0xefbeadde                           ; Used for generating jump greater than equal to far labels
template_end ificmpeqTemplate

template_start ifacmpeqTemplate
    _64bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _64bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11, r12                            ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    je 0xefbeadde                           ; Used for generating jump greater than equal to far labels
template_end ifacmpeqTemplate

; if_icmpne succeeds if and only if value1 ≠ value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ificmpneTemplate
    _32bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11d, r12d                          ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    jne 0xefbeadde                          ; Used for generating jump greater than equal to far labels
template_end ificmpneTemplate

; if_icmpne succeeds if and only if value1 ≠ value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ifacmpneTemplate
    _64bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _64bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11, r12                            ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    jne 0xefbeadde                          ; Used for generating jump greater than equal to far labels
template_end ifacmpneTemplate

; if_icmplt succeeds if and only if value1 < value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ificmpltTemplate
    _32bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11d, r12d                          ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    jl 0xefbeadde                           ; Used for generating jump greater than equal to far labels
template_end ificmpltTemplate

; if_icmple succeeds if and only if value1 < value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ificmpleTemplate
    _32bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11d, r12d                          ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    jle 0xefbeadde                          ; Used for generating jump greater than equal to far labels
template_end ificmpleTemplate

; if_icmpge succeeds if and only if value1 ≥ value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ificmpgeTemplate
    _32bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11d, r12d                          ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    jge 0xefbeadde                          ; Used for generating jump greater than equal to far labels
template_end ificmpgeTemplate

; if_icmpgt succeeds if and only if value1 > value2
; Stack before: ..., value1, value2 ->
; Stack after:  ...
template_start ificmpgtTemplate
    _32bit_slot_stack_to_rXX r12,0          ; pop first value off java stack into the value register
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    _32bit_slot_stack_to_rXX r11,0          ; pop second value off java stack into accumulator
    pop_single_slot                         ; reduce the stack size by 1 slot (8 bytes)
    cmp r11d, r12d                          ; r12 (minuend) - r11 (subtrahend) and set EFLAGS
    jg 0xefbeadde                           ; Used for generating jump greater than to far labels
template_end ificmpgtTemplate

template_start gotoTemplate
    jmp 0xefbeadde  ; Used for generating jump not equal to far labels
template_end gotoTemplate

; Below bytecodes are untested

; if_null succeeds if and only if value == 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifnullTemplate
    _64bit_slot_stack_to_rXX rax,0          ; grab the reference from the operand stack
    pop_single_slot                         ; reduce the mjit stack by one slot
    cmp rax, 0                              ; compare against 0 (null)
    je 0xefbeadde                           ; jump if rax is null
template_end ifnullTemplate

; if_nonnull succeeds if and only if value != 0
; Stack before: ..., value ->
; Stack after:  ...
template_start ifnonnullTemplate
    _64bit_slot_stack_to_rXX rax,0          ; grab the reference from the operand stack
    pop_single_slot                         ; reduce the mjit stack by one slot
    cmp rax, 0                              ; compare against 0 (null)
    jne 0xefbeadde                          ; jump if rax is not null
template_end ifnonnullTemplate
