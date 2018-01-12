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
        .set eq_pointer_size,4
        .set eqS_longVolatileRead,140
        .set eqS_longVolatileWrite,140
        .set eqSR_longVolatileRead,19
        .set eqSR_longVolatileWrite,19
        .set eqSRS_longVolatileRead,76
        .set eqSRS_longVolatileWrite,76
        .set eqSS_longVolatileRead,560
        .set eqSS_longVolatileWrite,560
        .section ".rodata"
        .global longVolatileWrite
        .type longVolatileWrite@function
        .global longVolatileRead
        .type longVolatileRead@function
# Prototype: U_64 longVolatileRead(J9VMThread * vmThread, U_64 * srcAddress);
# Defined in: #UTIL Args: 2
        .section ".text"
        .align 2
longVolatileRead:
        # offset 0 - 2 magic slots for back chain in callee's frame (used by callee)
        # offset 8 - 16 slots for max call space
        # offset 72 - 8 slots for outparm marshalling area
        # offset 104 - 19 slots for preserved int regs
        # offset 180 - 140 slots for locals
        # offset 740 - 3 slots for extra stack space
        # offset 752 - next stack frame
        mflr r0        	       	        	              	# Load link register into r0
        stw r0,4(r1)
        stwu r1,-(192+eqSS_longVolatileRead)(r1)        	              	# Back link pointer
        stw r13,104(r1)
        stw r14,108(r1)
        stw r15,112(r1)
        stw r16,116(r1)
        stw r17,120(r1)
        stw r18,124(r1)
        stw r19,128(r1)
        stw r20,132(r1)
        stw r21,136(r1)
        stw r22,140(r1)
        stw r23,144(r1)
        stw r24,148(r1)
        stw r25,152(r1)
        stw r26,156(r1)
        stw r27,160(r1)
        stw r28,164(r1)
        stw r29,168(r1)
        stw r30,172(r1)
        stw r31,176(r1)
        addi r0,r1,(200+eqSS_longVolatileRead)   #  previous frame = 50 = 45 fixed + 3 extra + 2 added by caller optimized to ternary
        bl _GLOBAL_OFFSET_TABLE_@local-4
        mflr r29
# Optimized case: Running 32 bit vm on 64 bit hw can use FP registers
        lfd fp3,0(r4)
# Find and align volatileSwapArea
        addi r3,r1,(7+180+eqSS_longVolatileRead) #   optimized to ternary
        addi r31,r0,-8                           # Loading constant(A) = 16rFFFFFFF8
        and r3,r3,r31
        stfd fp3,0(r3)
# Lay down read barrier here (and also break apart lfd lwz lwz dispatch group)
        isync
        lwz r5,0(r3)
        lwz r0,4(r3)
        ori r4,r0, 0
        ori r3,r5, 0
        lwz r13,104(r1)
        lwz r14,108(r1)
        lwz r15,112(r1)
        lwz r16,116(r1)
        lwz r17,120(r1)
        lwz r18,124(r1)
        lwz r19,128(r1)
        lwz r20,132(r1)
        lwz r21,136(r1)
        lwz r22,140(r1)
        lwz r23,144(r1)
        lwz r24,148(r1)
        lwz r25,152(r1)
        lwz r26,156(r1)
        lwz r27,160(r1)
        lwz r28,164(r1)
        lwz r29,168(r1)
        lwz r30,172(r1)
        lwz r31,176(r1)
        lwz r1,0(r1)    	              	        	# load savedSP
        lwz r0,4(r1)    	              	        	# load savedLR
        mtlr r0
        blr
# Prototype: void longVolatileWrite(J9VMThread * vmThread, U_64 * destAddress, U_64 * value);
# Defined in: #UTIL Args: 3
        .section ".text"
        .align 2
longVolatileWrite:
        # offset 0 - 2 magic slots for back chain in callee's frame (used by callee)
        # offset 8 - 16 slots for max call space
        # offset 72 - 8 slots for outparm marshalling area
        # offset 104 - 19 slots for preserved int regs
        # offset 180 - 140 slots for locals
        # offset 740 - 3 slots for extra stack space
        # offset 752 - next stack frame
        mflr r0        	       	        	              	# Load link register into r0
        stw r0,4(r1)
        stwu r1,-(192+eqSS_longVolatileWrite)(r1)       	              	# Back link pointer
        stw r13,104(r1)
        stw r14,108(r1)
        stw r15,112(r1)
        stw r16,116(r1)
        stw r17,120(r1)
        stw r18,124(r1)
        stw r19,128(r1)
        stw r20,132(r1)
        stw r21,136(r1)
        stw r22,140(r1)
        stw r23,144(r1)
        stw r24,148(r1)
        stw r25,152(r1)
        stw r26,156(r1)
        stw r27,160(r1)
        stw r28,164(r1)
        stw r29,168(r1)
        stw r30,172(r1)
        stw r31,176(r1)
        addi r0,r1,(eqSS_longVolatileWrite+200)  #  previous frame = 50 = 45 fixed + 3 extra + 2 added by caller optimized to ternary
        bl _GLOBAL_OFFSET_TABLE_@local-4
        mflr r29
        stw r4,728(r1)                           # save inParm2
        lwz r4,0(r5)
        lwz r0,4(r5)
# Optimized case: Running 32 bit vm on 64 bit hw can use FP registers
# Find and align volatileSwapArea
        addi r3,r1,(7+eqSS_longVolatileWrite+180) #   optimized to ternary
        addi r31,r0,-8                           # Loading constant(A) = 16rFFFFFFF8
        and r3,r3,r31
        stw r4,0(r3)
        stw r0,4(r3)
# Lay down write barrier (also break apart stw stw lfd dispatch group)
        lwsync
        lfd fp3,0(r3)
        lwz r4,728(r1)                           # load inParm2
        stfd fp3,0(r4)
# Lay down read-write barrier
        sync
        lwz r13,104(r1)
        lwz r14,108(r1)
        lwz r15,112(r1)
        lwz r16,116(r1)
        lwz r17,120(r1)
        lwz r18,124(r1)
        lwz r19,128(r1)
        lwz r20,132(r1)
        lwz r21,136(r1)
        lwz r22,140(r1)
        lwz r23,144(r1)
        lwz r24,148(r1)
        lwz r25,152(r1)
        lwz r26,156(r1)
        lwz r27,160(r1)
        lwz r28,164(r1)
        lwz r29,168(r1)
        lwz r30,172(r1)
        lwz r31,176(r1)
        lwz r1,0(r1)    	              	        	# load savedSP
        lwz r0,4(r1)    	              	        	# load savedLR
        mtlr r0
        blr
