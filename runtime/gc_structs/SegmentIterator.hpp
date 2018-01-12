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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(SEGMENTITERATOR_HPP_)
#define SEGMENTITERATOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "j9.h"


/**
 * Iterate over a linked list of memory segments.
 * @ingroup GC_Structs
 */
class GC_SegmentIterator
{
	J9MemorySegment *_memorySegment;
	uintptr_t _flags;

public:

	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; }
	
	/**
	 * @param memorySegment the list of segments to iterate over
	 * @flags for every segment returned by nextSegment(), all bits in <code>flags</code>
	 * will also be in the <code>type</code> field of that segment.
	 */
	GC_SegmentIterator(J9MemorySegmentList *memorySegmentList, uintptr_t flags) :
		_memorySegment(memorySegmentList->nextSegment),
		_flags(flags)
	{};

	/**
	 * @param memorySegment the first segment in the list
	 * @flags for every segment returned by nextSegment(), all bits in <code>flags</code>
	 * will also be in the <code>type</code> field of that segment.
	 */	
	GC_SegmentIterator(J9MemorySegment *memorySegment, uintptr_t flags) :
		_memorySegment(memorySegment),
		_flags(flags)
	{};	

	J9MemorySegment *nextSegment();
};

#endif /* SEGMENTITERATOR_HPP_ */

