
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#if !defined(CLASSLOADERSEGMENTITERATOR_HPP_)
#define CLASSLOADERSEGMENTITERATOR_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"


/**
 * Iterate over a linked list of classloader memory segments.
 * @ingroup GC_Structs
 */
class GC_ClassLoaderSegmentIterator
{
	J9MemorySegment *_memorySegment;
	UDATA _flags;

public:

	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; }
	
	/**
	 * @param memorySegment the list of segments to iterate over
	 * @flags for every segment returned by nextSegment(), all bits in <code>flags</code>
	 * will also be in the <code>type</code> field of that segment.
	 */
	GC_ClassLoaderSegmentIterator(J9ClassLoader *classLoader, UDATA flags) :
		_memorySegment(classLoader->classSegments),
		_flags(flags)
	{};

	J9MemorySegment *nextSegment();
};

#endif /* SEGMENTITERATOR_HPP_ */

