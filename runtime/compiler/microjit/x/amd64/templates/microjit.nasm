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

%macro pop_single_slot 0
add r10, 8
%endmacro

%macro pop_dual_slot 0
add r10, 16
%endmacro

%macro push_single_slot 0
sub r10, 8
%endmacro

%macro push_dual_slot 0
sub r10, 16
%endmacro

%macro _32bit_local_to_rXX_PATCH 1
mov %1d, dword [r14 + 0xefbeadde]
%endmacro

%macro _64bit_local_to_rXX_PATCH 1
mov %{1}, qword [r14 + 0xefbeadde]
%endmacro

%macro _32bit_local_from_rXX_PATCH 1
mov dword [r14 + 0xefbeadde], %1d
%endmacro

%macro _64bit_local_from_rXX_PATCH 1
mov qword [r14 + 0xefbeadde], %1
%endmacro

%macro _32bit_slot_stack_to_rXX 2
mov %1d, dword [r10 + %2]
%endmacro

%macro _32bit_slot_stack_to_eXX 2
mov e%{1}, dword [r10 + %2]
%endmacro

%macro _64bit_slot_stack_to_rXX 2
mov %{1}, qword [r10 + %2]
%endmacro

%macro _32bit_slot_stack_from_rXX 2
mov dword [r10 + %2], %1d
%endmacro

%macro _32bit_slot_stack_from_eXX 2
mov  dword [r10 + %2], e%1
%endmacro

%macro _64bit_slot_stack_from_rXX 2
mov qword [r10 + %2], %1
%endmacro
