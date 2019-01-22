; Copyright (c) 2000, 2019 IBM Corp. and others
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

segment .text

        DECLARE_GLOBAL  compressString
        DECLARE_GLOBAL  compressStringJ
        DECLARE_GLOBAL  compressStringNoCheck
        DECLARE_GLOBAL  compressStringNoCheckJ
        DECLARE_GLOBAL  andORString

        align 16


compressString:
        shr  rcx, 4
        add  rsi, rax
        add  rsi, rax
        mov  rdx, 0
eightchars:
        mov  rax, QWORD  [rsi]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rdx, rax				    ;
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+2], ax			; write the low 2 bytes of the 4 byte value in the destination

        mov  rax, QWORD  [rsi+8]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rdx, rax				    ;
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi+4], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+6], ax			; write the low 2 bytes of the 4 byte value in the destination

        mov  rax, QWORD  [rsi+16]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rdx, rax				    ;
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi+8], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+10], ax			; write the low 2 bytes of the 4 byte value in the destination


        mov  rax, QWORD  [rsi+24]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rdx, rax				    ;
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi+12], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+14], ax			; write the low 2 bytes of the 4 byte value in the destination

        add  rdi, 16
        add  rsi, 32		
        loop eightchars
                mov  rax, rdx
                shl  rdx, 32
                or   rax, rdx
                ;loop eightchars
        shr  rax, 32
ret



; A c-style memmove with no assumptions on the element size
; or copy direction required.
; ecx has length of copy in bytes
; esi has source address
; edi has destination address
compressStringJ:
        shr  rcx, 4
        add  rsi, rax
        add  rsi, rax
        mov  rdx, 0
eightcharsJ:
        mov  rax, QWORD  [rsi]  		; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, QWORD  [rsi+8]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rdx, rax				    ;
        or   rdx, rbx
        shl  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  QWORD  [rdi], rax			; write the low 2 bytes of the 4 byte value in the destination

        mov  rax, QWORD  [rsi+16]  		; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, QWORD  [rsi+24]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rdx, rax				    ;
        or   rdx, rbx
        shl  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  QWORD  [rdi+8], rax		; write the low 2 bytes of the 4 byte value in the destination

        add  rdi, 16
        add  rsi, 32		
        loop eightcharsJ
                mov  rax, rdx
                shl  rdx, 32
                or   rax, rdx
        shr  rax, 32
ret

andORString:
        shr  rcx, 4
        add  rsi, rax
        add  rsi, rax
        mov  rbx, 0
        mov  rdx, 0ffffffffffffffffh
eightchars2:
        mov  rax, QWORD  [rsi]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rbx, rax
        and  rdx, rax
        mov  rax, QWORD  [rsi+8]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rbx, rax
        and  rdx, rax
        mov  rax, QWORD  [rsi+16]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rbx, rax
        and  rdx, rax
        mov  rax, QWORD  [rsi+24]  		; load 4 bytes from the source array into EAX (2 chars)
        or   rbx, rax
        and  rdx, rax
        add  rsi, 32		
        loop eightchars2

        mov  rax, rbx                           ; building the or
        mov  rcx, rdx                           ; building the and
        shr  rbx, 32                            ; building the or
        shr  rdx, 32                            ; building the and
        or   ebx, eax				            ; building the or
        and  edx, ecx				            ; building the and
        mov  eax, ebx				            ; building the or
        mov  ecx, edx				            ; building the and
        shr  ebx, 16				            ; building the or
        shl  edx, 16				            ; building the and
        or   bx, ax				                ; building the or
        and  edx, ecx				            ; building the and

        mov  dx, bx
ret


compressStringNoCheckJ:
        shr  rcx, 4
        add  rsi, rax
        add  rsi, rax
eightcharsNoCheckJ:
        mov  rax, QWORD  [rsi]  		        ; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, QWORD  [rsi+8]  		        ; load 4 bytes from the source array into EAX (2 chars)
        shl  rbx, 8					            ; shift right by 8
        or   rax, rbx					        ; or the 2 values
        mov  QWORD  [rdi], rax			        ; write the low 2 bytes of the 4 byte value in the destination

        mov  rax, QWORD  [rsi+16]  		        ; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, QWORD  [rsi+24]  		        ; load 4 bytes from the source array into EAX (2 chars)
        shl  rbx, 8					            ; shift right by 8
        or   rax, rbx					        ; or the 2 values
        mov  QWORD  [rdi+8], rax			    ; write the low 2 bytes of the 4 byte value in the destination

        add  rdi, 16
        add  rsi, 32		
        loop eightcharsNoCheckJ
ret

compressStringNoCheck:
        shr  rcx, 4
        add  rsi, rax
        add  rsi, rax
eightcharsNoCheck:
        mov  rax, QWORD  [rsi]  		; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+2], ax			; write the low 2 bytes of the 4 byte value in the destination

        mov  rax, QWORD  [rsi+8]  		; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi+4], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+6], ax			; write the low 2 bytes of the 4 byte value in the destination

        mov  rax, QWORD  [rsi+16]  		; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi+8], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+10], ax			; write the low 2 bytes of the 4 byte value in the destination


        mov  rax, QWORD  [rsi+24]  		; load 4 bytes from the source array into EAX (2 chars)
        mov  rbx, rax					; copy the loaded value
        shr  rbx, 8					    ; shift right by 8
        or   rax, rbx					; or the 2 values
        mov  WORD  [rdi+12], ax			; write the low 2 bytes of the 4 byte value in the destination
        shr  rax, 32
        mov  WORD  [rdi+14], ax			; write the low 2 bytes of the 4 byte value in the destination

        add  rdi, 16
        add  rsi, 32		
        loop eightcharsNoCheck
ret
