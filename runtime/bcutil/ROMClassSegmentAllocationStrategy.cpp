/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
 * ROMClassSegmentAllocationStrategy.cpp
 */

#include "ROMClassSegmentAllocationStrategy.hpp"
#include "ut_j9bcu.h"

U_8*
ROMClassSegmentAllocationStrategy::allocate(UDATA bytesRequired)
{
	U_8* result = NULL;

	/* Scan existing segments for one large enough to hold the new ROM class */

	J9MemorySegment* segment = NULL;
	bool isLoadedByAnonClassLoader = _classLoader == _javaVM->anonClassLoader;
	/* always make a new segment if its an anonClass */
	if (!isLoadedByAnonClassLoader) {
		J9MemorySegmentList* classSegments = _javaVM->classMemorySegments;
#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_enter(classSegments->segmentMutex);
#endif
		segment = _classLoader->classSegments;

		while (NULL != segment) {
			if ((segment->type & (MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_ALLOCATED)) == (MEMORY_TYPE_ROM_CLASS | MEMORY_TYPE_ALLOCATED)) {
				UDATA romAvailable = segment->heapTop - segment->heapAlloc;

				if (romAvailable >= bytesRequired) {
					result = segment->heapAlloc;
					break;
				}
			}
			segment = segment->nextSegmentInClassLoader;
		}
#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_exit(classSegments->segmentMutex);
#endif
	}

	/* If no segment was found which could hold the new ROM class, allocate a new one */

	if (NULL == result) {
		UDATA classAllocationIncrement = _javaVM->romClassAllocationIncrement;
		if (isLoadedByAnonClassLoader) {
			classAllocationIncrement = 0;
		}
		segment = _javaVM->internalVMFunctions->allocateClassMemorySegment(_javaVM, bytesRequired, MEMORY_TYPE_DYNAMIC_LOADED_CLASSES, _classLoader, classAllocationIncrement);
		if (segment != NULL) {
			result = segment->heapAlloc;
		}
	}

	if ( NULL != result ) {
		segment->heapAlloc += bytesRequired;
		/*
		 * store a reference to the used segment and bytesRequested
		 * so that heapAlloc can later be adjusted to
		 * the actual amount of memory used in updateFinalROMSize()
		 */
		_segment = segment;
		_bytesRequested = bytesRequired;
	}

	return result;
}

void
ROMClassSegmentAllocationStrategy::updateFinalROMSize(UDATA finalSize)
{
	Trc_BCU_Assert_NotEquals( NULL, _segment );
	/* this assumes that the appropriate lock is held from allocate() to  now */
	_segment->heapAlloc -= _bytesRequested;
	_segment->heapAlloc += finalSize;
}
