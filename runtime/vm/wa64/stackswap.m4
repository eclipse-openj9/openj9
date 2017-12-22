dnl Copyright (c) 1991, 2017 IBM Corp. and others
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

eqS_swapStacksAndRunHandler equ 43
eqSR_swapStacksAndRunHandler equ 8
eqSRS_swapStacksAndRunHandler equ 64
eqSS_swapStacksAndRunHandler equ 344
       	CONST SEGMENT 'CONST'
       	CONST ends
       	_TEXT SEGMENT 'CODE'
        extrn vmSignalHandler:near
        public swapStacksAndRunHandler

        align 16
swapStacksAndRunHandler 	PROC
        push RBP
        mov RBP, RSP
        push RBX
        push RSI
        push RDI
        push R12
        push R13
        push R14
        push R15
        sub RSP, 344
        mov qword ptr (0+8+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)[RSP],RCX
        mov qword ptr (0+16+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)[RSP],RDX
        mov qword ptr (0+eqSRS_swapStacksAndRunHandler+24+eqSS_swapStacksAndRunHandler)[RSP],R8
        mov qword ptr (0+32+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)[RSP],R9
        mov qword ptr 8[RSP],RBP
        mov R12,RSP
        mov RAX,qword ptr J9TR_VMThread_entryLocalStorage[R9]
        mov RSP,qword ptr J9TR_ELS_machineSPSaveSlot[RAX]
        sub RSP,32
        mov RBP,qword ptr(32+8)[RSP]
        call vmSignalHandler
        add RSP,32
        mov RSP,R12
        add RSP, 344
        pop R15
        pop R14
        pop R13
        pop R12
        pop RDI
        pop RSI
        pop RBX
        pop RBP
        ret
swapStacksAndRunHandler        ENDP
       	_TEXT ends
       	end
