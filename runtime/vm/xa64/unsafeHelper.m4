dnl Copyright (c) 1991, 2018 IBM Corp. and others
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

	DECLARE_PUBLIC(unsafePut64)
	DECLARE_PUBLIC(unsafePut32)
	DECLARE_PUBLIC(unsafePut16)
	DECLARE_PUBLIC(unsafePut8)
	DECLARE_PUBLIC(unsafeGet64)
	DECLARE_PUBLIC(unsafeGet32)
	DECLARE_PUBLIC(unsafeGet16)
	DECLARE_PUBLIC(unsafeGet8)

dnl Prototype: void unsafePut64(I_64 *address, I_64 value);
dnl Defined in: #Args: 2

START_PROC(unsafePut64)
	push rbp
	mov rbp, rsp
	mov [rdi], rsi
	pop rbp
	ret
END_PROC(unsafePut64)

dnl Prototype: void unsafePut32(I_32 *address, I_32 value);
dnl Defined in: #Args: 2

START_PROC(unsafePut32)
	push rbp
	mov rbp, rsp
	mov [rdi], esi
	pop rbp
	ret
END_PROC(unsafePut32)

dnl Prototype: void unsafePut16(I_16 *address, I_16 value);
dnl Defined in: #Args: 2

START_PROC(unsafePut16)
	push rbp
	mov rbp, rsp
	mov [rdi], si
	pop rbp
	ret
END_PROC(unsafePut16)

dnl Prototype: void unsafePut8(I_8 *address, I_8 value);
dnl Defined in: #Args: 2

START_PROC(unsafePut8)
	push rbp
	mov rbp, rsp
	mov [rdi], sil
	pop rbp
	ret
END_PROC(unsafePut8)

dnl Prototype: I_64 unsafeGet64(I_64 *address);
dnl Defined in: #Args: 1

START_PROC(unsafeGet64)
	push rbp
	mov rbp, rsp
	mov rax, [rdi]
	pop rbp
	ret
END_PROC(unsafeGet64)

dnl Prototype: I_32 unsafeGet32(I_32 *address);
dnl Defined in: #Args: 1

START_PROC(unsafeGet32)
	push rbp
	mov rbp, rsp
	mov eax, [rdi]
	pop rbp
	ret
END_PROC(unsafeGet32)

dnl Prototype: I_16 unsafeGet16(I_16 *address);
dnl Defined in: #Args: 1

START_PROC(unsafeGet16)
	push rbp
	mov rbp, rsp
	mov eax, [rdi]
	pop rbp
	ret
END_PROC(unsafeGet16)

dnl Prototype: I_8 unsafeGet8(I_8 *address);
dnl Defined in: #Args: 1

START_PROC(unsafeGet8)
	push rbp
	mov rbp, rsp
	mov eax, [rdi]
	pop rbp
	ret
END_PROC(unsafeGet8)

	FILE_END

