
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

#include "j9.h"
#include "j9cfg.h"
#include "jni.h"
#include "j9protos.h"

#include "AllocationFailureStats.hpp"
#include "GCExtensions.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "VMInterface.hpp"

extern "C" {
#define QUERY_GC_STATUS_NUMBER_OF_HEAPS_SCAVENGER_ENABLED 2
#define QUERY_GC_STATUS_NUMBER_OF_HEAPS_SCAVENGER_DISABLED 1
/**
 * Query the state of the Garbage Collector.
 * 
 * Initially, the API should be called with a statusSize of 0.  This call will return
 * JNI_EINVAL and nHeaps, which is the number of storage structures for which data will be
 * returned.  This allows the caller to calculate how large statusSize should be by using
 * something similar to nHeaps * sizeof(GCStatus).
 * 
 * The API returns the data in the supplied array and can be interrogated using the heap field
 * to identify whether the data is for a heap or shared memory.  If the data is for shared memory,
 * the count field can be used to identify whether the data is for the shared memory page pool
 * or for a subpool.  There is no implied order in which the data is returned.
 * 
 * @param vm The javaVM
 * @param nHeap Number of heaps for which data has been returned
 * @param status An array of GCStatus structures
 * @param statusSize The size of the status array in bytes
 * 
 * @return JNI_OK Call completed successfully
 * @return JNI_ERR Processing the request failed
 * @return JNI_EINVAL The query given was invalid, for example, the size of the status array is too small.
 */
jint JNICALL
queryGCStatus(JavaVM *vm, jint *nHeaps, GCStatus *status, jint statusSize)
{
	J9JavaVM *javaVM = (J9JavaVM *)vm;
	jint filledElements = *nHeaps; /* Number of elements of status that are filled */
	jint returnCode = JNI_OK;

	/* may not be attached to a VM */
/*
	if (omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT)) {
		return JNI_ERR;
	}
	javaVM->internalVMFunctions->acquireExclusiveVMAccessFromExternalThread(javaVM);
*/

	/* From here on in, must branch to exit to return.  Do not return directly */
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_MemorySpace *memorySpace = extensions->heap->getMemorySpaceList();

	/* first call? */
	if (0 == statusSize) {
		/* Return JNI_EINVAL and update nHeaps to be the number of storage structures
		 * that will be populated.
		 */
		*nHeaps = 0;
		while (memorySpace) {
#if defined (J9VM_GC_MODRON_SCAVENGER)
			if (extensions->scavengerEnabled) {
				*nHeaps += QUERY_GC_STATUS_NUMBER_OF_HEAPS_SCAVENGER_ENABLED;
			} else
#endif /* J9VM_GC_MODRON_SCAVENGER */
			{
				*nHeaps += QUERY_GC_STATUS_NUMBER_OF_HEAPS_SCAVENGER_DISABLED;
			}

			memorySpace = memorySpace->getNext();
		}
		returnCode = JNI_EINVAL;
		goto done;
	}

	/* Sanity check, it may be some incorrect large number, but it can never be negative */
	if (*nHeaps < 0) {
		returnCode = JNI_EINVAL;
		goto done;
	}

	/* Sanity check, it may be some incorrect large number, but it can never be negative */
	if (statusSize < 0) {
		returnCode = JNI_EINVAL;
		goto done;
	}

	/* Sanity check, are there enough array elements to hold the requested data? */
	if (statusSize != (*nHeaps) * (jint)sizeof(GCStatus)) {
		returnCode = JNI_EINVAL;
		goto done;
	}

	/* Initialize the return structure to known values */
	memset((void *)status, 0, statusSize);
	while ((NULL != memorySpace) && (0 != filledElements)) {
#if defined(J9VM_GC_MODRON_SCAVENGER)
		if (extensions->scavengerEnabled) {
			MM_MemorySubSpace *defaultMemorySubSpace = memorySpace->getDefaultMemorySubSpace();
			MM_AllocationFailureStats *allocationFailureStats = defaultMemorySubSpace->getAllocationFailureStats();
			status->heap = (jint) JNI_GCQUERY_NURSERY_HEAP;
			status->count = (jint) allocationFailureStats->allocationFailureCount;
			status->freestorage = (jlong) defaultMemorySubSpace->getActualFreeMemorySize();
			status->totalstorage = (jlong) defaultMemorySubSpace->getActiveMemorySize();
			status += 1;
			filledElements -=1;
		}
#endif /* J9VM_GC_MODRON_SCAVENGER */

		MM_MemorySubSpace *tenuredMemorySubSpace = memorySpace->getTenureMemorySubSpace();
		MM_AllocationFailureStats *tenuredAllocationFailureStats = tenuredMemorySubSpace->getAllocationFailureStats();
		status->heap = (jint) JNI_GCQUERY_MATURE_HEAP;
		status->count = (jint) tenuredAllocationFailureStats->allocationFailureCount;
		status->freestorage = (jlong) tenuredMemorySubSpace->getActualFreeMemorySize();
		status->totalstorage = (jlong) tenuredMemorySubSpace->getActiveMemorySize();
		status += 1;
		filledElements -=1;

		memorySpace = memorySpace->getNext();
	}

	/* Yet another sanity check.  If there is a mismatch between the expected number
	 * of elements to be returned, and the actual number of elements filled
	 * inform the user.  Don't wipe the data already collected.
	 */
	if ((NULL != memorySpace) || (0 != filledElements)) {
		returnCode = JNI_EINVAL;
	}

done:

/*
	javaVM->internalVMFunctions->releaseExclusiveVMAccessFromExternalThread(javaVM);
	omrthread_detach(NULL);
*/
	return returnCode;
}

} /* extern "C" */
 
