# Copyright (c) 2019, 2020 IBM Corp. and others
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
	.align  2
	.globl  unsafePut64
	.type   unsafePut64, @function
unsafePut64:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	sd      a1,-32(s0)
	ld      a5,-24(s0)
	ld      a4,-32(s0)
	sd      a4,0(a5)
	nop
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafePut64, .-unsafePut64

# Prototype: void unsafePut32(I_32 *address, I_32 value);
# Defined in: #Args: 2
	.align  2
	.globl  unsafePut32
	.type   unsafePut32, @function
unsafePut32:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	mv      a5,a1
	sw      a5,-28(s0)
	ld      a5,-24(s0)
	lw      a4,-28(s0)
	sw      a4,0(a5)
	nop
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafePut32, .-unsafePut32

# Prototype: void unsafePut16(I_16 *address, I_16 value);
# Defined in: #Args: 2
	.align  2
	.globl  unsafePut16
	.type   unsafePut16, @function
unsafePut16:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	mv      a5,a1
	sh      a5,-26(s0)
	ld      a5,-24(s0)
	lhu     a4,-26(s0)
	sh      a4,0(a5)
	nop
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafePut16, .-unsafePut16

# Prototype: void unsafePut8(I_8 *address, I_8 value);
# Defined in: #Args: 2
	.align  2
	.globl  unsafePut8
	.type   unsafePut8, @function
unsafePut8:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	mv      a5,a1
	sb      a5,-25(s0)
	ld      a5,-24(s0)
	lbu     a4,-25(s0)
	sb      a4,0(a5)
	nop
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafePut8, .-unsafePut8

# Prototype: I_64 unsafeGet64(I_64 *address);
# Defined in: #Args: 1
	.align  2
	.globl  unsafeGet64
	.type   unsafeGet64, @function
unsafeGet64:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	ld      a5,-24(s0)
	ld      a5,0(a5)
	mv      a0,a5
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafeGet64, .-unsafeGet64

# Prototype: I_32 unsafeGet32(I_32 *address);
# Defined in: #Args: 1
	.align  2
	.globl  unsafeGet32
	.type   unsafeGet32, @function
unsafeGet32:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	ld      a5,-24(s0)
	lw      a5,0(a5)
	mv      a0,a5
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafeGet32, .-unsafeGet32

# Prototype: I_16 unsafeGet16(I_16 *address);
# Defined in: #Args: 1
	.align  2
	.globl  unsafeGet16
	.type   unsafeGet16, @function
unsafeGet16:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	ld      a5,-24(s0)
	lh      a5,0(a5)
	mv      a0,a5
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafeGet16, .-unsafeGet16

# Prototype: I_8 unsafeGet8(I_8 *address);
# Defined in: #Args: 1
	.align  2
	.globl  unsafeGet8
	.type   unsafeGet8, @function
unsafeGet8:
	addi    sp,sp,-32
	sd      s0,24(sp)
	addi    s0,sp,32
	sd      a0,-24(s0)
	ld      a5,-24(s0)
	lb      a5,0(a5)
	mv      a0,a5
	ld      s0,24(sp)
	addi    sp,sp,32
	jr      ra
	.size   unsafeGet8, .-unsafeGet8
