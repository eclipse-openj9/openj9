; Copyright (c) 2010, 2017 IBM Corp. and others
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
; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

	.686p
	assume cs:flat,ds:flat,ss:flat
	.xmm

	_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'

	public J9SSE2cpuidFeatures

	align 16

; Prototype: U_32 J9SSE2cpuidFeatures(void);
J9SSE2cpuidFeatures PROC NEAR
	; test if cpuid is supported on this CPU
	pushfd			      ; get EFLAGS content (assuming EFLAGS exists!)
	pop	eax
	mov	ecx, eax	      ; make a backup of the original EFLAGS content
	xor	eax, 200000h	; flip ID flag
	push	eax		      ; replace current EFLAGS content
	popfd
	pushfd			      ; check EFLAGS again
	pop	eax
	xor eax, ecx	      ; compare updated value with backup
	je	L1                ; processor does not cpuid

	push ebx
	mov eax, 1
	cpuid
	mov eax, edx
	pop ebx
	ret

L1:
	; return all zeros (i.e. no capabilities)
	xor eax, eax
	ret

J9SSE2cpuidFeatures ENDP


_TEXT ends
END
