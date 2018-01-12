# Copyright (c) 1991, 2017 IBM Corp. and others
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
        .globl unsafePut64
        .type unsafePut64,@function
        .globl unsafePut32
        .type unsafePut32,@function
        .globl unsafePut16
        .type unsafePut16,@function
        .globl unsafePut8
        .type unsafePut8,@function
        .globl unsafeGet64
        .type unsafeGet64,@function
        .globl unsafeGet32
        .type unsafeGet32,@function
        .globl unsafeGet16
        .type unsafeGet16,@function
        .globl unsafeGet8
        .type unsafeGet8,@function

## Prototype: void unsafePut64(I_64 *address, I_64 value);
## Defined in: #Args: 2
     	.text
        .align 3
unsafePut64:
        std 4,0(3)
		blr
END_unsafePut64:
        .size unsafePut64,END_unsafePut64 - unsafePut64

## Prototype: void unsafePut32(I_32 *address, I_32 value);
## Defined in: #Args: 2
        .text
        .align 3
unsafePut32:
        stw 4,0(3)
        blr
END_unsafePut32:
        .size unsafePut32,END_unsafePut32 - unsafePut32

## Prototype: void unsafePut16(I_16 *address, I_16 value);
## Defined in: #Args: 2
        .text
        .align 3
unsafePut16:
		sth 4,0(3)
		blr
END_unsafePut16:
        .size unsafePut16,END_unsafePut16 - unsafePut16

## Prototype: void unsafePut8(I_8 *address, I_8 value);
## Defined in: #Args: 2
        .text
        .align 3
unsafePut8:
		stb 4,0(3)
		blr
END_unsafePut8:
        .size unsafePut8,END_unsafePut8 - unsafePut8

        ## Prototype: I_64 unsafeGet64(I_64 *address);
## Defined in: #Args: 1
        .text
        .align 3
unsafeGet64:
        ld 3,0(3)
        blr
END_unsafeGet64:
        .size unsafeGet64,END_unsafeGet64 - unsafeGet64

## Prototype: I_32 unsafeGet32(I_32 *address);
## Defined in: #Args: 1
        .text
        .align 3
unsafeGet32:
        lwa 3,0(3)
        blr
END_unsafeGet32:
        .size unsafeGet32,END_unsafeGet32 - unsafeGet32

## Prototype: I_16 unsafeGet16(I_16 *address);
## Defined in: #Args: 1
        .text
        .align 3
unsafeGet16:
        lha 3,0(3)
        blr
END_unsafeGet16:
        .size unsafeGet16,END_unsafeGet16 - unsafeGet16

## Prototype: I_8 unsafeGet8(I_8 *address);
## Defined in: #Args: 1
        .text
        .align 3
unsafeGet8:
        lbz 3,0(3)
        extsb 3,3
        blr
END_unsafeGet8:
        .size unsafeGet8,END_unsafeGet8 - unsafeGet8

       	#CODE32 ends
        # end of file
