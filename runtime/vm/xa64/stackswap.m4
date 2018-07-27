dnl Copyright (c) 2017, 2018 IBM Corp. and others
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

include(xhelpers.m4)

	FILE_START


eqS_swapStacksAndRunHandler = 33
eqSR_swapStacksAndRunHandler = 6
eqSRS_swapStacksAndRunHandler = 48
eqSS_swapStacksAndRunHandler = 264
        DECLARE_PUBLIC(vmSignalHandler)
        DECLARE_PUBLIC(swapStacksAndRunHandler)

dnl Intel syntax: instruction dest source
START_PROC(swapStacksAndRunHandler)
        nop
        push rbp
        mov rbp, rsp
        push rbx
        push r12
        push r13
        push r14
        push r15
        sub rsp,264
        mov 8[rsp], rbp
        mov r12, rsp
        mov rax, J9TR_VMThread_entryLocalStorage[rcx]
        mov rsp, J9TR_ELS_machineSPSaveSlot[rax]
        xor rax, rax
        mov rbp, 8[rsp]
ifdef({OSX},{
        call C_FUNCTION_SYMBOL(vmSignalHandler)
},{
        call vmSignalHandler@PLT
})
dnl XMM registers are not saved / restored.  Does that matter here?
        nop
        mov rsp, r12
        add rsp,264
        pop r15
        pop r14
        pop r13
        pop r12
        pop rbx
        pop rbp
        ret
END_PROC(swapStacksAndRunHandler)

	FILE_END
