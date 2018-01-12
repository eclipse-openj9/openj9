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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
# GPL-2.0 WITH Classpath-exception-2.0 OR
# LicenseRef-GPL-2.0 WITH Assembly-exception
        .file "volatile.s"
        .set r0,0
        .set fp0,0
        .set r1,1
        .set fp1,1
        .set r2,2
        .set fp2,2
        .set r3,3
        .set fp3,3
        .set r4,4
        .set fp4,4
        .set r5,5
        .set fp5,5
        .set r6,6
        .set fp6,6
        .set r7,7
        .set fp7,7
        .set r8,8
        .set fp8,8
        .set r9,9
        .set fp9,9
        .set r10,10
        .set fp10,10
        .set r11,11
        .set fp11,11
        .set r12,12
        .set fp12,12
        .set r13,13
        .set fp13,13
        .set r14,14
        .set fp14,14
        .set r15,15
        .set fp15,15
        .set SL_BORROW,0B0100
        .set SL_NO_BORROW,0B0011
        .set AL_CARRY_SET,0B0011
        .set AL_CARRY_CLEAR,0B1100
        .set ALWAYS,0B1111
.set eq_inline_jni_max_arg_count,32
.set eq_pointer_size,4
.set eqS_longVolatileRead,69
.set eqS_longVolatileWrite,69
.set eqSR_longVolatileRead,10
.set eqSR_longVolatileWrite,10
.set eqSRS_longVolatileRead,40
.set eqSRS_longVolatileWrite,40
.set eqSS_longVolatileRead,276
.set eqSS_longVolatileWrite,276
        .globl longVolatileWrite
        .globl longVolatileRead
# Prototype: U_64 longVolatileRead(J9VMThread * vmThread, U_64 * srcAddress);
# Defined in: #UTIL Args: 2

        .text
        .align 4
.globl longVolatileRead
longVolatileRead:

        .type longVolatileRead,@function
        #112 slots = 24 fixed + 16 out-args + 69 locals + 3 extra + 0 alignment
        stm  r6,r15,24(r15)                      # save volatile registers
        lr   r0,r15                               # remember oldSP
        ahi  r15,-448                            # buy a stack frame
        st   r0,0(r15)                            # update SP backchain
        lr   r2,r3
        lm   r0,r1,0(r2)
        lr   r3,r1
        lr   r2,r0
        lm   r6,r15,472(r15)                      # restore volatile registers (including returnAddress and oldSP)
        br   r14
END_longVolatileRead:
        .size longVolatileRead,END_longVolatileRead - longVolatileRead
# Prototype: void longVolatileWrite(J9VMThread * vmThread, U_64 * destAddress, U_64 * value);
# Defined in: #UTIL Args: 3

        .text
        .align 4
.globl longVolatileWrite
longVolatileWrite:

        .type longVolatileWrite,@function
        #112 slots = 24 fixed + 16 out-args + 69 locals + 3 extra + 0 alignment
        stm  r6,r15,24(r15)                      # save volatile registers
        lr   r0,r15                               # remember oldSP
        ahi  r15,-448                            # buy a stack frame
        st   r0,0(r15)                            # update SP backchain
        lr   r5,r3
        l    r1,0(r4)
        l    r2,4(r4)
        lr   r0,r1
        lr   r1,r2
        stm  r0,r1,0(r5)
        bcr ALWAYS,r0 # readwrite barrier
        lm   r6,r15,472(r15)                      # restore volatile registers (including returnAddress and oldSP)
        br   r14
END_longVolatileWrite:
        .size longVolatileWrite,END_longVolatileWrite - longVolatileWrite
