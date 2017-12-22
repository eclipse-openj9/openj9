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
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

include(jilvalues.m4)

eqS_swapStacksAndRunHandler = 16
eqSR_swapStacksAndRunHandler = 4
eqSRS_swapStacksAndRunHandler = 16
eqSS_swapStacksAndRunHandler = 64
        .globl vmSignalHandler
        .globl swapStacksAndRunHandler
        .type swapStacksAndRunHandler,@function

        .text
        .align 4
swapStacksAndRunHandler:
        push %ebp
        mov %esp, %ebp
        push %esi
        push %edi
        push %ebx
        sub $64, %esp
        call .L2
.L1:
        jmp .L3
.L2:
        movl (%esp),%ebx
        ret
.L3:
        addl $_GLOBAL_OFFSET_TABLE_+[.-.L1],%ebx
        movl %ebp, 4(%esp)
        movl (0+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler+4)(%esp), %eax
        movl (0+8+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)(%esp), %ebp
        movl (0+eqSRS_swapStacksAndRunHandler+12+eqSS_swapStacksAndRunHandler)(%esp), %ecx
        movl (0+16+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)(%esp), %edx
        movl %esp, %esi
        movl J9TR_VMThread_entryLocalStorage(%edx), %edi
        movl J9TR_ELS_machineSPSaveSlot(%edi), %esp
        pushl %edx
        pushl %ecx
        pushl %ebp
        pushl %eax
        movl (16+4)(%esp), %ebp
        call vmSignalHandler@PLT
        addl $16, %esp
        movl %esi, %esp
        add $64, %esp
        pop %ebx
        pop %edi
        pop %esi
        pop %ebp
        ret
END_swapStacksAndRunHandler:
        .size swapStacksAndRunHandler,END_swapStacksAndRunHandler - swapStacksAndRunHandler
