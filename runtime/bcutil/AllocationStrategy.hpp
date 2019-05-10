/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

/*
 * AllocationStrategy.hpp
 */

#ifndef ALLOCATIONSTRATEGY_HPP_
#define ALLOCATIONSTRATEGY_HPP_

/* @ddr_namespace: default */
#include "j9comp.h"

class AllocationStrategy
{
public:
	struct AllocatedBuffers {
		U_8 *romClassBuffer;
		U_8 *lineNumberBuffer;
		U_8 *variableInfoBuffer;
	};

	/* 
	 * allocate a single memory buffer 
	 * */
	virtual U_8* allocate(UDATA byteAmount) = 0;

	/* 
	 * allocate multiple memory regions to accommodate out of line debug information
	 * with sizes as specified by the argument list.
	 */
	virtual bool allocateWithOutOfLineData(AllocatedBuffers *allocatedBuffers, UDATA byteAmount, UDATA lineNumberByteAmount, UDATA variableInfoByteAmount) { return false; }
	
	/* 
	 * Indicate the actual amount of data used, allowing the allocation provider to
	 * reclaim buffer space.  This applies to the single memory buffer allocator
	 * and the 'romClassBuffer' of the multiple allocator.
	 */
	virtual void updateFinalROMSize(UDATA finalSize) = 0;
	
	/* 
	 * Returns true if the allocator can possibly provide multiple buffers to store 
	 * debug information out of line.
	 */
	virtual bool canStoreDebugDataOutOfLine() { return false; }
};

#endif /* ALLOCATIONSTRATEGY_HPP_ */
