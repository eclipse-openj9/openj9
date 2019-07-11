/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef memcheck_h
#define memcheck_h

/* @ddr_namespace: default */
typedef struct J9MemoryCheckHeader {
	UDATA wrappedBlockSize;
	UDATA allocationNumber;

	/* IF MODE is J9_MCMODE_MPROTECT */
	U_8 *self;						/* Ptr returned by malloc; must be given to free */
	U_8 *topPage;					/* start of Top Locked Page */
	U_8 *bottomPage;			/* start of (TODO - ?most? Bottom Locked Page */
	U_8 *wrappedBlock;		/* start of the block given to user */
	IDATA isLocked;				/* Current Lock state */
	UDATA totalAllocation;		/* Total amount allocated */
	/* END IF */

	struct J9MemoryCheckHeader *nextBlock;
	struct J9MemoryCheckHeader *previousBlock;
	J9MEMAVLTreeNode *node;
} J9MemoryCheckHeader;

typedef struct J9MemoryCheckStats {
	/* Current state. */

	UDATA totalBlocksAllocated;
	UDATA totalBlocksFreed;
	U_64 totalBytesAllocated;
	U_64 totalBytesFreed;
	UDATA largestBlockAllocated;
	UDATA largestBlockAllocNum;
	UDATA totalUnknownBlocksIgnored;

	UDATA currentBlocksAllocated;
	UDATA hiWaterBlocksAllocated;
	UDATA currentBytesAllocated;
	UDATA hiWaterBytesAllocated;	

	UDATA failedAllocs;

} J9MemoryCheckStats;

#endif     /* memcheck_h */

