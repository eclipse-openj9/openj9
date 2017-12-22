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
	.686p
	assume cs:flat,ds:flat,ss:flat
	.xmm

	_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

	public J9SSE2memclear

	align 16

J9SSE2memclear PROC NEAR
	mov EAX, dword ptr 4[ESP]
	mov ECX, dword ptr 8[ESP]
	shr ecx, 7	; 128 bytes per

	xorpd xmm0,xmm0

loop1:

	movntdq  0[EAX], xmm0
	movntdq 16[EAX], xmm0
	movntdq 32[EAX], xmm0
	movntdq 48[EAX], xmm0
	movntdq 64[EAX], xmm0
	movntdq 80[EAX], xmm0
	movntdq 96[EAX], xmm0
	movntdq 112[EAX], xmm0

	add EAX, 128
	dec ecx
	jnz loop1 

	sfence
	ret

J9SSE2memclear ENDP

_TEXT ends
END
