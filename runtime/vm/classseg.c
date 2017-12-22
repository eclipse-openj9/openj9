
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

/**
 * @file
 * @ingroup VM
 */

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"

#include "vm_api.h"

const U_32 GROWTH_START = 1; /**< start growing from the GROWTH_START allocation */
const U_32 GROWTH_STEPS = 5; /**< grow over GROWTH_STEPS allocations */

/**
 * Calculate appropriate segment size for the loader.
 * 
 * The appropriate size is at least as large as the required size and is some
 * power-of-two fraction (i.e. 1/2 the size, 1/4, etc.) of the preferred allocation
 * increment. For class loaders other than the system and application class loaders:
 * the appropriate size of the first GROWTH_START segments for the given class loader
 * is exactly the required size; and the size of the next GROWTH_STEPS segments grows
 * exponentially up to the preferred allocation increment.
 * 
 * @param requiredSize Minimum segment size required
 * @param segmentType Type of segment required
 * @param classLoader Class loader requesting class segment
 * @param allocationIncrement Preferred segment allocation size
 * @return A segment size at least as large as requiredSize
 */
static UDATA 
calculateAppropriateSegmentSize(J9JavaVM *javaVM, UDATA requiredSize, UDATA segmentType, J9ClassLoader *classLoader, UDATA allocationIncrement)
{
	UDATA segmentSize = allocationIncrement;

	/* The order of these conditions is based on their relative failure frequency when starting Eclipse 3.2M3 */
	if ((classLoader != javaVM->systemClassLoader)
	&& (requiredSize < segmentSize)
	&& (classLoader != javaVM->applicationClassLoader)
	&& (classLoader != javaVM->anonClassLoader)
	) {
		UDATA allocatedSegments = 0;
		UDATA allocatedType = segmentType | MEMORY_TYPE_ALLOCATED;
		J9MemorySegment *segment = classLoader->classSegments;
	
		/* Count the segments for this classLoader.  If the count becomes == GROWTH_START + GROWTH_STEPS
		 * stop since we handle all segment >= GROWTH_START + GROWTH_STEPS the same
		 */ 
		while (NULL != segment) {
			if (allocatedType == segment->type) {
				allocatedSegments++;
			}
			if (allocatedSegments == (GROWTH_START + GROWTH_STEPS)) {
				break;
			}
			segment = segment->nextSegmentInClassLoader;
	 	}
	 	
		/* Use alternative allocation strategy */
		if (GROWTH_START > allocatedSegments) {
			/* First GROWTH_START segments are an exact fit */
			segmentSize = requiredSize;
		} else if ((GROWTH_START + GROWTH_STEPS) > allocatedSegments) {
			segmentSize >>= (GROWTH_START + GROWTH_STEPS) - allocatedSegments;
		}
	}

	/* Ensure segment size at least satisfies the required size */
	return OMR_MAX(segmentSize, requiredSize);
}

/**
 * Allocate and return a class memory segment.
 * 
 * @param requiredSize Minimum segment size required
 * @param segmentType Type of segment required
 * @param classLoader Class loader requesting class segment
 * @param allocationIncrement Preferred segment allocation size
 * @return A newly allocated class memory segment
 * @note allocationIncrement should typically be javaVM->ramClassAllocationIncrement or javaVM->romClassAllocationIncrement
 */
J9MemorySegment *
allocateClassMemorySegment(J9JavaVM *javaVM, UDATA requiredSize, UDATA segmentType, J9ClassLoader *classLoader, UDATA allocationIncrement)
{
	UDATA appropriateSize = 0;
	J9MemorySegment *memorySegment = NULL;
	
#if defined(J9VM_THR_PREEMPTIVE)
	if (javaVM->classMemorySegments->segmentMutex) {
		omrthread_monitor_enter(javaVM->classMemorySegments->segmentMutex);
	}
#endif

	appropriateSize = calculateAppropriateSegmentSize(javaVM, requiredSize, segmentType, classLoader, allocationIncrement);
	memorySegment = allocateMemorySegmentInList(javaVM, javaVM->classMemorySegments, appropriateSize, segmentType, J9MEM_CATEGORY_CLASSES);
	
	 if (NULL != memorySegment) {
		memorySegment->classLoader = classLoader;
		memorySegment->nextSegmentInClassLoader = classLoader->classSegments;
		classLoader->classSegments = memorySegment;
	}
	
#if defined(J9VM_THR_PREEMPTIVE)
	if (javaVM->classMemorySegments->segmentMutex) {
		omrthread_monitor_exit(javaVM->classMemorySegments->segmentMutex);
	}
#endif

	return memorySegment;
}
