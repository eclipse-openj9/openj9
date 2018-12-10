/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.j9ddr.vm29.j9.stackwalker;

import com.ibm.j9ddr.vm29.j9.J9ConfigFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

/**
 * JIT register map.
 * 
 * Per-platform register information reproduced here rather than embedded in blob.
 * 
 * @author andhall
 */
public class JITRegMap {

	static final String jitRegisterNames[];

	static final int jitCalleeDestroyedRegisterList[];

	static final int jitCalleeSavedRegisterList[];

	static {
		if (J9ConfigFlags.arch_x86) {
			if (!J9BuildFlags.env_data64) {
				jitRegisterNames = new String[] {
						"jit_eax",
						"jit_ebx",
						"jit_ecx",
						"jit_edx",
						"jit_edi",
						"jit_esi",
						"jit_ebp"
				};

				jitCalleeDestroyedRegisterList = new int[] {
						0x00,   /* jit_eax */
						0x03,   /* jit_edx */
						0x04,   /* jit_edi */
						0x06    /* jit_ebp */
				};

				jitCalleeSavedRegisterList = new int[] {
						0x05,   /* jit_esi */
						0x02,   /* jit_ecx */
						0x01    /* jit_ebx */
				};
			} else {
				jitRegisterNames = new String[] {
						"jit_rax",
						"jit_rbx",
						"jit_rcx",
						"jit_rdx",
						"jit_rdi",
						"jit_rsi",
						"jit_rbp",
						"jit_rsp",
						"jit_r8",
						"jit_r9",
						"jit_r10",
						"jit_r11",
						"jit_r12",
						"jit_r13",
						"jit_r14",
						"jit_r15"
				};

				jitCalleeDestroyedRegisterList = new int[] {
						0x00,   /* jit_rax */
						0x02,   /* jit_rcx */
						0x03,   /* jit_rdx */
						0x04,   /* jit_rdi */
						0x05,   /* jit_rsi */
						0x06,   /* jit_rbp */
						0x07,   /* jit_rsp */
						0x08    /* jit_r8 */
				};

				jitCalleeSavedRegisterList = new int[] {
						0x01,   /* jit_rbx */
						0x0F,   /* jit_r15 */
						0x0E,   /* jit_r14 */
						0x0D,   /* jit_r13 */
						0x0C,   /* jit_r12 */
						0x0B,   /* jit_r11 */
						0x0A,   /* jit_r10 */
						0x09    /* jit_r9 */
				};
			}
		} else if (J9ConfigFlags.arch_power) {
			if (!J9BuildFlags.env_data64) {
				jitRegisterNames = new String[] {
						"jit_r0",
						"jit_r1",
						"jit_r2",
						"jit_r3",
						"jit_r4",
						"jit_r5",
						"jit_r6",
						"jit_r7",
						"jit_r8",
						"jit_r9",
						"jit_r10",
						"jit_r11",
						"jit_r12",
						"jit_r13",
						"jit_r14",
						"jit_r15",
						"jit_r16",
						"jit_r17",
						"jit_r18",
						"jit_r19",
						"jit_r20",
						"jit_r21",
						"jit_r22",
						"jit_r23",
						"jit_r24",
						"jit_r25",
						"jit_r26",
						"jit_r27",
						"jit_r28",
						"jit_r29",
						"jit_r30",
						"jit_r31"
				};

				jitCalleeDestroyedRegisterList = new int[] {
						0x00,   /* jit_r0 */
						0x01,   /* jit_r1 */
						0x02,   /* jit_r2 */
						0x03,   /* jit_r3 */
						0x04,   /* jit_r4 */
						0x05,   /* jit_r5 */
						0x06,   /* jit_r6 */
						0x07,   /* jit_r7 */
						0x08,   /* jit_r8 */
						0x09,   /* jit_r9 */
						0x0A,   /* jit_r10 */
						0x0B,   /* jit_r11 */
						0x0C,   /* jit_r12 */
						0x0D,   /* jit_r13 */
						0x0E    /* jit_r14 */
				};

				jitCalleeSavedRegisterList = new int[] {
						0x1F,   /* jit_r31 */
						0x1E,   /* jit_r30 */
						0x1D,   /* jit_r29 */
						0x1C,   /* jit_r28 */
						0x1B,   /* jit_r27 */
						0x1A,   /* jit_r26 */
						0x19,   /* jit_r25 */
						0x18,   /* jit_r24 */
						0x17,   /* jit_r23 */
						0x16,   /* jit_r22 */
						0x15,   /* jit_r21 */
						0x14,   /* jit_r20 */
						0x13,   /* jit_r19 */
						0x12,   /* jit_r18 */
						0x11,   /* jit_r17 */
						0x10,   /* jit_r16 */
						0x0F    /* jit_r15 */
				};
			} else {
				jitRegisterNames = new String[] {
						"jit_r0",
						"jit_r1",
						"jit_r2",
						"jit_r3",
						"jit_r4",
						"jit_r5",
						"jit_r6",
						"jit_r7",
						"jit_r8",
						"jit_r9",
						"jit_r10",
						"jit_r11",
						"jit_r12",
						"jit_r13",
						"jit_r14",
						"jit_r15",
						"jit_r16",
						"jit_r17",
						"jit_r18",
						"jit_r19",
						"jit_r20",
						"jit_r21",
						"jit_r22",
						"jit_r23",
						"jit_r24",
						"jit_r25",
						"jit_r26",
						"jit_r27",
						"jit_r28",
						"jit_r29",
						"jit_r30",
						"jit_r31"
				};

				jitCalleeDestroyedRegisterList = new int[] {
						0x00,   /* jit_r0 */
						0x01,   /* jit_r1 */
						0x02,   /* jit_r2 */
						0x03,   /* jit_r3 */
						0x04,   /* jit_r4 */
						0x05,   /* jit_r5 */
						0x06,   /* jit_r6 */
						0x07,   /* jit_r7 */
						0x08,   /* jit_r8 */
						0x09,   /* jit_r9 */
						0x0A,   /* jit_r10 */
						0x0B,   /* jit_r11 */
						0x0C,   /* jit_r12 */
						0x0D,   /* jit_r13 */
						0x0E,   /* jit_r14 */
						0x0F    /* jit_r15 */
				};

				jitCalleeSavedRegisterList = new int[] {
						0x1F,   /* jit_r31 */
						0x1E,   /* jit_r30 */
						0x1D,   /* jit_r29 */
						0x1C,   /* jit_r28 */
						0x1B,   /* jit_r27 */
						0x1A,   /* jit_r26 */
						0x19,   /* jit_r25 */
						0x18,   /* jit_r24 */
						0x17,   /* jit_r23 */
						0x16,   /* jit_r22 */
						0x15,   /* jit_r21 */
						0x14,   /* jit_r20 */
						0x13,   /* jit_r19 */
						0x12,   /* jit_r18 */
						0x11,   /* jit_r17 */
						0x10    /* jit_r16 */
				};
			}
		} else if (J9ConfigFlags.arch_s390) {
			if (!J9BuildFlags.env_data64) {
				jitRegisterNames = new String[] {
						"jit_r0",
						"jit_r1",
						"jit_r2",
						"jit_r3",
						"jit_r4",
						"jit_r5",
						"jit_r6",
						"jit_r7",
						"jit_r8",
						"jit_r9",
						"jit_r10",
						"jit_r11",
						"jit_r12",
						"jit_r13",
						"jit_r14",
						"jit_r15",
						"jit_r16",
						"jit_r17",
						"jit_r18",
						"jit_r19",
						"jit_r20",
						"jit_r21",
						"jit_r22",
						"jit_r23",
						"jit_r24",
						"jit_r25",
						"jit_r26",
						"jit_r27",
						"jit_r28",
						"jit_r29",
						"jit_r30",
						"jit_r31"
				};

				jitCalleeDestroyedRegisterList = new int[] {
						0x00,	/* jit_r0 */
						0x01,	/* jit_r1 */
						0x02,	/* jit_r2 */
						0x03,	/* jit_r3 */
						0x04,	/* jit_r4 */
						0x05,	/* jit_r5 */
						0x0D,	/* jit_r13 */
						0x0E,	/* jit_r14 */
						0x0F,	/* jit_r15 */
						0x10,	/* jit_r16 */
						0x11,	/* jit_r17 */
						0x12,	/* jit_r18 */
						0x13,	/* jit_r19 */
						0x14,	/* jit_r20 */
						0x15,	/* jit_r21 */
						0x1D,	/* jit_r29 */
						0x1E,	/* jit_r30 */
						0x1F	/* jit_r31 */
				};

				jitCalleeSavedRegisterList = new int[] {
						0x1C,	/* jit_r28 */
						0x1B,	/* jit_r27 */
						0x1A,	/* jit_r26 */
						0x19,	/* jit_r25 */
						0x18,	/* jit_r24 */
						0x17,	/* jit_r23 */
						0x16,	/* jit_r22 */
						0x0C,	/* jit_r12 */
						0x0B,	/* jit_r11 */
						0x0A,	/* jit_r10 */
						0x09,	/* jit_r9 */
						0x08,	/* jit_r8 */
						0x07,	/* jit_r7 */
						0x06	/* jit_r6 */
				};
			} else {
				/* 390 64 bit */
				jitRegisterNames = new String[] {
						"jit_r0",
						"jit_r1",
						"jit_r2",
						"jit_r3",
						"jit_r4",
						"jit_r5",
						"jit_r6",
						"jit_r7",
						"jit_r8",
						"jit_r9",
						"jit_r10",
						"jit_r11",
						"jit_r12",
						"jit_r13",
						"jit_r14",
						"jit_r15",
				};

				jitCalleeDestroyedRegisterList = new int[] {
						0x00,	/* jit_r0 */
						0x01,	/* jit_r1 */
						0x02,	/* jit_r2 */
						0x03,	/* jit_r3 */
						0x04,	/* jit_r4 */
						0x05,	/* jit_r5 */
						0x0D,	/* jit_r13 */
						0x0E,	/* jit_r14 */
						0x0F,	/* jit_r15 */
				};

				jitCalleeSavedRegisterList = new int[] {
						0x0C,	/* jit_r12 */
						0x0B,	/* jit_r11 */
						0x0A,	/* jit_r10 */
						0x09,	/* jit_r9 */
						0x08,	/* jit_r8 */
						0x07,	/* jit_r7 */
						0x06	/* jit_r6 */
				};
			}
		} else {
			throw new InternalError("Unsupported platform");
		}
	}

}
