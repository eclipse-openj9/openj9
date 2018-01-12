; Copyright (c) 1991, 2017 IBM Corp. and others
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
	.586p
       	assume cs:flat,ds:flat,ss:flat
       	.xmm
eq_pointer_size equ 4
eqS_longVolatileRead equ 16
eqS_longVolatileWrite equ 16
eqSR_longVolatileRead equ 2
eqSR_longVolatileWrite equ 2
eqSRS_longVolatileRead equ 8
eqSRS_longVolatileWrite equ 8
eqSS_longVolatileRead equ 64
eqSS_longVolatileWrite equ 64
       	CONST SEGMENT DWORD USE32 PUBLIC 'CONST'
       	CONST ends
       	_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
        public longVolatileWrite
        public longVolatileRead
; Prototype: U_64 longVolatileRead(J9VMThread * vmThread, U_64 * srcAddress);
; Defined in: #UTIL Args: 2
        align 16
longVolatileRead       	PROC NEAR
        ;  localStackUse = 16
        push EBP
        mov EBP,ESP
        push EBX
        sub ESP,76
        mov EBX,dword ptr (eqSRS_longVolatileRead+12+4+eqSS_longVolatileRead)[ESP]
        mov EBX,dword ptr (eqSRS_longVolatileRead+8+12+eqSS_longVolatileRead)[ESP]
        movq xmm0,qword ptr [EBX]
        movd ECX,xmm0
        pshufd xmm1, xmm0, 225
        movd EBX,xmm1
        mov EDX,EBX
        mov EAX,ECX
        add ESP,76
        pop EBX
        pop EBP
        ret
longVolatileRead        ENDP
; Prototype: void longVolatileWrite(J9VMThread * vmThread, U_64 * destAddress, U_64 * value);
; Defined in: #UTIL Args: 3
        align 16
longVolatileWrite      	PROC NEAR
        ;  localStackUse = 16
        push EBP
        mov EBP,ESP
        push EBX
        sub ESP,76
        mov EBX,dword ptr (eqSS_longVolatileWrite+12+4+eqSRS_longVolatileWrite)[ESP]
        mov EDX,dword ptr (eqSS_longVolatileWrite+8+12+eqSRS_longVolatileWrite)[ESP]
        mov EAX,dword ptr (eqSS_longVolatileWrite+(2*12)+eqSRS_longVolatileWrite)[ESP]
        mov ECX,dword ptr [EAX]
        mov EBX,dword ptr 4[EAX]
        movd xmm0,ECX
        movd xmm1,EBX
        punpckldq xmm0, xmm1
        movq qword ptr [EDX],xmm0
        add ESP,76
        pop EBX
        pop EBP
        ret
longVolatileWrite        ENDP
       	_TEXT ends
       	end
