dnl Copyright (c) 2017, 2017 IBM Corp. and others
dnl
dnl This program and the accompanying materials are made available under
dnl the terms of the Eclipse Public License 2.0 which accompanies this
dnl distribution and is available at https://www.eclipse.org/legal/epl-2.0/
dnl or the Apache License, Version 2.0 which accompanies this distribution and
dnl is available at https://www.apache.org/licenses/LICENSE-2.0.
dnl
dnl This Source Code may also be made available under the following
dnl Secondary Licenses when the conditions for such availability set
dnl forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
dnl General Public License, version 2 with the GNU Classpath
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] http://openjdk.java.net/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

include(jilvalues.m4)

eqS_swapStacksAndRunHandler = 33
eqSR_swapStacksAndRunHandler = 6
eqSRS_swapStacksAndRunHandler = 48
eqSS_swapStacksAndRunHandler = 264
        .globl vmSignalHandler
        .globl swapStacksAndRunHandler
        .type swapStacksAndRunHandler,@function

        .text
        .align 4
swapStacksAndRunHandler:
        nop
        push %rbp
        mov %rsp, %rbp
        push %rbx
        push %r12
        push %r13
        push %r14
        push %r15
        sub $264, %rsp
        movq %rbp, 8(%rsp)
        movq %rsp, %r12
        movq J9TR_VMThread_entryLocalStorage(%rcx), %rax
        movq J9TR_ELS_machineSPSaveSlot(%rax), %rsp
        xor %rax, %rax
        movq 8(%rsp), %rbp
        call vmSignalHandler@PLT
        nop
        movq %r12, %rsp
        add $264, %rsp
        pop %r15
        pop %r14
        pop %r13
        pop %r12
        pop %rbx
        pop %rbp
        ret
END_swapStacksAndRunHandler:
        .size swapStacksAndRunHandler,END_swapStacksAndRunHandler - swapStacksAndRunHandler
