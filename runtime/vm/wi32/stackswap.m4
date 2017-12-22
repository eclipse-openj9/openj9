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

		.586p
       	assume cs:flat,ds:flat,ss:flat
       	.xmm
eqS_swapStacksAndRunHandler equ 16
eqSR_swapStacksAndRunHandler equ 3
eqSRS_swapStacksAndRunHandler equ 12
eqSS_swapStacksAndRunHandler equ 64
       	CONST SEGMENT DWORD USE32 PUBLIC 'CONST'
       	CONST ends
       	_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
        extrn vmSignalHandler:near
        public swapStacksAndRunHandler

        align 16
swapStacksAndRunHandler 	PROC NEAR
        push EBP
        mov EBP,ESP
        push ESI
        push EDI
        sub ESP,64
        mov dword ptr 4[ESP],EBP
        mov EAX,dword ptr (0+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler+4)[ESP]
        mov EBP,dword ptr (0+8+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)[ESP]
        mov ECX,dword ptr (0+eqSRS_swapStacksAndRunHandler+12+eqSS_swapStacksAndRunHandler)[ESP]
        mov EDX,dword ptr (0+16+eqSRS_swapStacksAndRunHandler+eqSS_swapStacksAndRunHandler)[ESP]
        mov ESI,ESP
        mov EDI,dword ptr J9TR_VMThread_entryLocalStorage[EDX]
        mov ESP,dword ptr J9TR_ELS_machineSPSaveSlot[EDI]
        push EDX
        push ECX
        push EBP
        push EAX
        mov EBP,dword ptr(16+4)[ESP]
        call vmSignalHandler
        add ESP,16
        mov ESP,ESI
        add ESP,64
        pop EDI
        pop ESI
        pop EBP
        ret
swapStacksAndRunHandler        ENDP
       	_TEXT ends
       	end
