# Copyright (c) 2019, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

	.text

# Prototype: void unsafePut64(I_64 *address, I_64 value);
# Defined in: #Args: 2
	.align	2
	.global	unsafePut64
	.type	unsafePut64, %function
unsafePut64:
	str	x1, [x0]
	ret
	.size	unsafePut64, .-unsafePut64

# Prototype: void unsafePut32(I_32 *address, I_32 value);
# Defined in: #Args: 2
	.align	2
	.global	unsafePut32
	.type	unsafePut32, %function
unsafePut32:
	str	w1, [x0]
	ret
	.size	unsafePut32, .-unsafePut32

# Prototype: void unsafePut16(I_16 *address, I_16 value);
# Defined in: #Args: 2
	.align	2
	.global	unsafePut16
	.type	unsafePut16, %function
unsafePut16:
	strh	w1, [x0]
	ret
	.size	unsafePut16, .-unsafePut16

# Prototype: void unsafePut8(I_8 *address, I_8 value);
# Defined in: #Args: 2
	.align	2
	.global	unsafePut8
	.type	unsafePut8, %function
unsafePut8:
	strb	w1, [x0]
	ret
	.size	unsafePut8, .-unsafePut8

# Prototype: I_64 unsafeGet64(I_64 *address);
# Defined in: #Args: 1
	.align	2
	.global	unsafeGet64
	.type	unsafeGet64, %function
unsafeGet64:
	ldr	x0, [x0]
	ret
	.size	unsafeGet64, .-unsafeGet64

# Prototype: I_32 unsafeGet32(I_32 *address);
# Defined in: #Args: 1
	.align	2
	.global	unsafeGet32
	.type	unsafeGet32, %function
unsafeGet32:
	ldr	w0, [x0]
	ret
	.size	unsafeGet32, .-unsafeGet32

# Prototype: I_16 unsafeGet16(I_16 *address);
# Defined in: #Args: 1
	.align	2
	.global	unsafeGet16
	.type	unsafeGet16, %function
unsafeGet16:
	ldrsh	w0, [x0]
	ret
	.size	unsafeGet16, .-unsafeGet16

# Prototype: I_8 unsafeGet8(I_8 *address);
# Defined in: #Args: 1
	.align	2
	.global	unsafeGet8
	.type	unsafeGet8, %function
unsafeGet8:
	ldrsb	w0, [x0]
	ret
	.size	unsafeGet8, .-unsafeGet8
