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
        .file "volatile.s"
        .set r0,0
        .set r1,1
        .set r2,2
        .set r3,3
        .set r4,4
        .set r5,5
        .set r6,6
        .set r7,7
        .set r8,8
        .set r9,9
        .set r10,10
        .set r11,11
        .set r12,12
        .set r13,13
        .set r14,14
        .set r15,15
        .set r16,16
        .set r17,17
        .set r18,18
        .set r19,19
        .set r20,20
        .set r21,21
        .set r22,22
        .set r23,23
        .set r24,24
        .set r25,25
        .set r26,26
        .set r27,27
        .set r28,28
        .set r29,29
        .set r30,30
        .set r31,31
        .set fp0,0
        .set fp1,1
        .set fp2,2
        .set fp3,3
        .set fp4,4
        .set fp5,5
        .set fp6,6
        .set fp7,7
        .set fp8,8
        .set fp9,9
        .set fp10,10
        .set fp11,11
        .set fp12,12
        .set fp13,13
        .set fp14,14
        .set fp15,15
        .set fp16,16
        .set fp17,17
        .set fp18,18
        .set fp19,19
        .set fp20,20
        .set fp21,21
        .set fp22,22
        .set fp23,23
        .set fp24,24
        .set fp25,25
        .set fp26,26
        .set fp27,27
        .set fp28,28
        .set fp29,29
        .set fp30,30
        .set fp31,31
        .extern ._ptrgl[pr]

        .set eq_pointer_size,4
        .set eqS_longVolatileRead,138
        .set eqS_longVolatileWrite,138
        .set eqSR_longVolatileRead,19
        .set eqSR_longVolatileWrite,19
        .set eqSRS_longVolatileRead,76
        .set eqSRS_longVolatileWrite,76
        .set eqSS_longVolatileRead,552
        .set eqSS_longVolatileWrite,552
        .toc
TOC.static: .tc .static[tc],_static[ro]
        .csect _static[ro]
        .globl .longVolatileRead[pr]
        .globl longVolatileRead[ds]
        .globl .longVolatileRead
        .toc
TOC.longVolatileRead: .tc .longVolatileRead[tc],longVolatileRead[ds]
        .csect longVolatileRead[ds]
        .long .longVolatileRead[pr]
        .long  TOC[tc0]
        .long  0
        .globl .longVolatileWrite[pr]
        .globl longVolatileWrite[ds]
        .globl .longVolatileWrite
        .toc
TOC.longVolatileWrite: .tc .longVolatileWrite[tc],longVolatileWrite[ds]
        .csect longVolatileWrite[ds]
        .long .longVolatileWrite[pr]
        .long  TOC[tc0]
        .long  0
# Prototype: U_64 longVolatileRead(J9VMThread * vmThread, U_64 * srcAddress);
# Defined in: #UTIL Args: 2
        .csect .longVolatileRead[pr]
        .function .longVolatileRead[pr],startproc.longVolatileRead,16,0,(endproc.longVolatileRead-startproc.longVolatileRead)
        startproc.longVolatileRead:
        mflr r0
        stw r0,8(r1) # save return address
        stw r31,-8(r1)
        stw r30,-4(r1)
        addi r30,r1,0
        stwu r1,-672(r1)
        stw r2,20(r1)
        stw r3,24(r30)
        stw r4,28(r30)
# Optimized case: Running 32 bit vm on 64 bit hw can use FP registers
        lfd fp3,0(r4)
# Find and align volatileSwapArea
        addi r3,r1,(7+88)                        #   optimized to ternary
        li r31,-8                                # Loading constant(A) = 16rFFFFFFF8
        and r3,r3,r31
        stfd fp3,0(r3)
# Lay down read barrier here (and also break apart lfd lwz lwz dispatch group)
        isync
        lwz r5,0(r3)
        lwz r0,4(r3)
        ori r4,r0, 0
        ori r3,r5, 0
        lwz r1,0(r1)
        lwz r31,-8(r1)
        lwz r30,-4(r1)
        lwz r0,8(r1)
        mtlr r0
        blr

        endproc.longVolatileRead:
# Prototype: void longVolatileWrite(J9VMThread * vmThread, U_64 * destAddress, U_64 * value);
# Defined in: #UTIL Args: 3
        .csect .longVolatileWrite[pr]
        .function .longVolatileWrite[pr],startproc.longVolatileWrite,16,0,(endproc.longVolatileWrite-startproc.longVolatileWrite)
        startproc.longVolatileWrite:
        mflr r0
        stw r0,8(r1) # save return address
        stw r31,-8(r1)
        stw r30,-4(r1)
        addi r30,r1,0
        stwu r1,-672(r1)
        stw r2,20(r1)
        stw r3,24(r30)
        stw r4,28(r30)
        stw r5,32(r30)
        lwz r6,0(r5)
        lwz r0,4(r5)
# Optimized case: Running 32 bit vm on 64 bit hw can use FP registers
# Find and align volatileSwapArea
        addi r3,r1,(7+88)                        #   optimized to ternary
        li r31,-8                                # Loading constant(A) = 16rFFFFFFF8
        and r3,r3,r31
        stw r6,0(r3)
        stw r0,4(r3)
# Lay down write barrier (also break apart stw stw lfd dispatch group)
        lwsync
        lfd fp3,0(r3)
        stfd fp3,0(r4)
# Lay down read-write barrier
        sync
        lwz r1,0(r1)
        lwz r31,-8(r1)
        lwz r30,-4(r1)
        lwz r0,8(r1)
        mtlr r0
        blr

        endproc.longVolatileWrite:
