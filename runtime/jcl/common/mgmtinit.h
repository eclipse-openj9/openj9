/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
#ifndef mgmtinit_h
#define mgmtinit_h

#include "omrgcconsts.h"
#include "j9modron.h"

#define THRESHOLD_EXCEEDED 1
#define COLLECTION_THRESHOLD_EXCEEDED 2
#define END_OF_GARBAGE_COLLECTION 3
#define NOTIFIER_SHUTDOWN_REQUEST 4

/* these constants have to match the logic in com.ibm.lang.management.OperatingSystemNotificationThread */
#define DLPAR_NOTIFIER_SHUTDOWN_REQUEST 0
#define DLPAR_NUMBER_OF_CPUS_CHANGE 1
#define DLPAR_PROCESSING_CAPACITY_CHANGE 2
#define DLPAR_TOTAL_PHYSICAL_MEMORY_CHANGE 3

#define NOTIFICATION_QUEUE_MAX 10

#define J9VM_MANAGEMENT_POOL_NONHEAP 0
#define J9VM_MANAGEMENT_POOL_NONHEAP_SEG 2
#define J9VM_MANAGEMENT_POOL_NONHEAP_SEG_CLASSES J9VM_MANAGEMENT_POOL_NONHEAP_SEG
#define J9VM_MANAGEMENT_POOL_NONHEAP_SEG_MISC (J9VM_MANAGEMENT_POOL_NONHEAP_SEG_CLASSES + 1)
#define J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_CODE (J9VM_MANAGEMENT_POOL_NONHEAP_SEG_MISC + 1)
#define J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_DATA (J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_CODE + 1)

#define J9VM_MANAGEMENT_POOL_HEAP 0x10000
#define J9VM_MANAGEMENT_POOL_HEAP_ID_MASK 0xffff

#define J9VM_MANAGEMENT_POOL_HEAP_JAVAHEAP (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_JAVAHEAP)

#define J9VM_MANAGEMENT_POOL_HEAP_TENURED (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_TENURED)
#define J9VM_MANAGEMENT_POOL_HEAP_TENURED_SOA (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_TENURED_SOA)
#define J9VM_MANAGEMENT_POOL_HEAP_TENURED_LOA (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_TENURED_LOA)
#define J9VM_MANAGEMENT_POOL_HEAP_NURSERY_ALLOCATE (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_NURSERY_ALLOCATE)
#define J9VM_MANAGEMENT_POOL_HEAP_NURSERY_SURVIVOR (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_NURSERY_SURVIVOR)

#define J9VM_MANAGEMENT_POOL_HEAP_REGION_OLD (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_REGION_OLD)
#define J9VM_MANAGEMENT_POOL_HEAP_REGION_EDEN (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_REGION_EDEN)
#define J9VM_MANAGEMENT_POOL_HEAP_REGION_SURVIVOR (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_REGION_SURVIVOR)
#define J9VM_MANAGEMENT_POOL_HEAP_REGION_RESERVED (J9VM_MANAGEMENT_POOL_HEAP + J9_GC_MANAGEMENT_POOL_REGION_RESERVED)

#define J9VM_MANAGEMENT_GC_HEAP 0x10000
#define J9VM_MANAGEMENT_GC_LOCAL 0x100
#define J9VM_MANAGEMENT_GC_HEAP_ID_MASK 0xff

#define J9VM_MANAGEMENT_GC_ID_GLOBAL (J9VM_MANAGEMENT_GC_HEAP + J9_GC_MANAGEMENT_COLLECTOR_GLOBAL)
#define J9VM_MANAGEMENT_GC_ID_SCAVENGE (J9VM_MANAGEMENT_GC_HEAP + J9VM_MANAGEMENT_GC_LOCAL + J9_GC_MANAGEMENT_COLLECTOR_SCAVENGE)
#define J9VM_MANAGEMENT_GC_ID_PGC (J9VM_MANAGEMENT_GC_HEAP + J9VM_MANAGEMENT_GC_LOCAL + J9_GC_MANAGEMENT_COLLECTOR_PGC)
#define J9VM_MANAGEMENT_GC_ID_GGC (J9VM_MANAGEMENT_GC_HEAP + J9_GC_MANAGEMENT_COLLECTOR_GGC)
#define J9VM_MANAGEMENT_GC_ID_EPSILON (J9VM_MANAGEMENT_GC_HEAP + J9_GC_MANAGEMENT_COLLECTOR_EPSILON)

#define J9VM_MANAGEMENT_NONHEAPPOOL_NAME_CLASSES "class storage"
#define J9VM_MANAGEMENT_NONHEAPPOOL_NAME_MISC "miscellaneous non-heap storage"
#define J9VM_MANAGEMENT_NONHEAPPOOL_NAME_JITCODE "JIT code cache"
#define J9VM_MANAGEMENT_NONHEAPPOOL_NAME_JITDATA "JIT data cache"

typedef struct memoryPoolUsageThreshold {
	U_32 poolID;
	U_64 usedSize;
	U_64 totalSize;
	U_64 maxSize;
	U_64 thresholdCrossingCount;
} memoryPoolUsageThreshold;

typedef struct J9MemoryNotification {
	UDATA type;
	U_64 sequenceNumber;
	struct J9MemoryNotification *next;
	memoryPoolUsageThreshold *usageThreshold;
	J9GarbageCollectionInfo *gcInfo;
} J9MemoryNotification;



typedef struct J9DLPARNotification {
	UDATA type;
	struct J9DLPARNotification *next;
	U_64 data;
	U_64 sequenceNumber;
} J9DLPARNotification;
jint managementInit(J9JavaVM *vm);
void managementTerminate(J9JavaVM *vm);


#define MEMORY_SEGMENT_LIST_DO(segmentList, imageSegment) {\
	J9MemorySegment *imageSegment, *i2; \
	imageSegment = segmentList->nextSegment; \
	while(imageSegment) { \
		i2 = (J9MemorySegment *)(imageSegment->nextSegment);
#define END_MEMORY_SEGMENT_LIST_DO(imageSegment) \
		imageSegment = i2; }}

#endif     /* mgmtinit_h */


