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
        .globl .unsafePut64[pr]
        .globl unsafePut64[ds]
        .globl .unsafePut64
        .toc
TOC.unsafePut64: .tc .unsafePut64[tc],unsafePut64[ds]
        .csect unsafePut64[ds],3
        .llong .unsafePut64[pr]
        .llong  TOC[tc0]
        .llong  0
		.globl .unsafePut32[pr]
        .globl unsafePut32[ds]
        .globl .unsafePut32
        .toc
TOC.unsafePut32: .tc .unsafePut32[tc],unsafePut32[ds]
        .csect unsafePut32[ds],3
        .llong .unsafePut32[pr]
        .llong  TOC[tc0]
        .llong  0
		.globl .unsafePut16[pr]
        .globl unsafePut16[ds]
        .globl .unsafePut16
        .toc
TOC.unsafePut16: .tc .unsafePut16[tc],unsafePut16[ds]
        .csect unsafePut16[ds],3
        .llong .unsafePut16[pr]
        .llong  TOC[tc0]
        .llong  0
		.globl .unsafePut8[pr]
        .globl unsafePut8[ds]
        .globl .unsafePut8
        .toc
TOC.unsafePut8: .tc .unsafePut8[tc],unsafePut8[ds]
        .csect unsafePut8[ds],3
        .llong .unsafePut8[pr]
        .llong  TOC[tc0]
        .llong  0
        .globl .unsafeGet64[pr]
        .globl unsafeGet64[ds]
        .globl .unsafeGet64
        .toc
TOC.unsafeGet64: .tc .unsafeGet64[tc],unsafeGet64[ds]
        .csect unsafeGet64[ds],3
        .llong .unsafeGet64[pr]
        .llong  TOC[tc0]
        .llong  0
        .globl .unsafeGet32[pr]
        .globl unsafeGet32[ds]
        .globl .unsafeGet32
        .toc
TOC.unsafeGet32: .tc .unsafeGet32[tc],unsafeGet32[ds]
        .csect unsafeGet32[ds],3
        .llong .unsafeGet32[pr]
        .llong  TOC[tc0]
        .llong  0
        .globl .unsafeGet16[pr]
        .globl unsafeGet16[ds]
        .globl .unsafeGet16
        .toc
TOC.unsafeGet16: .tc .unsafeGet16[tc],unsafeGet16[ds]
        .csect unsafeGet16[ds],3
        .llong .unsafeGet16[pr]
        .llong  TOC[tc0]
        .llong  0
        .globl .unsafeGet8[pr]
        .globl unsafeGet8[ds]
        .globl .unsafeGet8
        .toc
TOC.unsafeGet8: .tc .unsafeGet8[tc],unsafeGet8[ds]
        .csect unsafeGet8[ds],3
        .llong .unsafeGet8[pr]
        .llong  TOC[tc0]
        .llong  0

## Prototype: void unsafePut64(I_64 *address, I_64 value);
## Defined in: #Args: 2
	.csect .unsafePut64[pr],3
	.unsafePut64:
	.function .unsafePut64,startproc.unsafePut64,16,0,(endproc.unsafePut64-startproc.unsafePut64)
	startproc.unsafePut64:
	std 4,0(3)
	blr
	endproc.unsafePut64:

## Prototype: void unsafePut32(I_32 *address, I_32 value);
## Defined in: #Args: 2
	.csect .unsafePut32[pr],3
	.unsafePut32:
	.function .unsafePut32,startproc.unsafePut32,16,0,(endproc.unsafePut32-startproc.unsafePut32)
	startproc.unsafePut32:
	stw 4,0(3)
	blr
	endproc.unsafePut32:

## Prototype: void unsafePut16(I_16 *address, I_16 value);
## Defined in: #Args: 2
	.csect .unsafePut16[pr],3
	.unsafePut16:
	.function .unsafePut16,startproc.unsafePut16,16,0,(endproc.unsafePut16-startproc.unsafePut16)
	startproc.unsafePut16:
	sth 4,0(3)
	blr
	endproc.unsafePut16:

## Prototype: void unsafePut8(I_8 *address, I_8 value);
## Defined in: #Args: 2
	.csect .unsafePut8[pr],3
	.unsafePut8:
	.function .unsafePut8,startproc.unsafePut8,16,0,(endproc.unsafePut8-startproc.unsafePut8)
	startproc.unsafePut8:
	stb 4,0(3)
	blr
	endproc.unsafePut8:

## Prototype: I_64 unsafeGet64(I_64 *address);
## Defined in: #Args: 1
	.csect .unsafeGet64[pr],3
	.unsafeGet64:
	.function .unsafeGet64,startproc.unsafeGet64,16,0,(endproc.unsafeGet64-startproc.unsafeGet64)
	startproc.unsafeGet64:
	ld 3,0(3)
	blr
	endproc.unsafeGet64:

## Prototype: I_32 unsafeGet32(I_32 *address);
## Defined in: #Args: 1
	.csect .unsafeGet32[pr],3
	.unsafeGet32:
	.function .unsafeGet32,startproc.unsafeGet32,16,0,(endproc.unsafeGet32-startproc.unsafeGet32)
	startproc.unsafeGet32:
	lwa 3,0(3)
	blr
	endproc.unsafeGet32:

## Prototype: I_16 unsafeGet16(I_16 *address);
## Defined in: #Args: 1
	.csect .unsafeGet16[pr],3
	.unsafeGet16:
	.function .unsafeGet16,startproc.unsafeGet16,16,0,(endproc.unsafeGet16-startproc.unsafeGet16)
	startproc.unsafeGet16:
	lha 3,0(3)
	blr
	endproc.unsafeGet16:

## Prototype: I_8 unsafeGet8(I_8 *address);
## Defined in: #Args: 1
	.csect .unsafeGet8[pr],3
	.unsafeGet8:
	.function .unsafeGet8,startproc.unsafeGet8,16,0,(endproc.unsafeGet8-startproc.unsafeGet8)
	startproc.unsafeGet8:
	lbz 0,0(3)
	extsb 3,0
	blr
	endproc.unsafeGet8:

       	#CODE32 ends
        # end of file
