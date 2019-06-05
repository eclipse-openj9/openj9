
/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * @ingroup GC_Base
 */

#ifndef MODRONAPI_HPP_
#define MODRONAPI_HPP_


#include "j9.h"
#include "j9cfg.h"
#include "modronapicore.hpp"

extern "C" {
/* define constant memory pool name and garbage collector name here -- the name should not be over 31 characters */
#define J9_GC_MANAGEMENT_POOL_NAME_HEAP_OLD				"Java heap"
#define J9_GC_MANAGEMENT_POOL_NAME_HEAP					"JavaHeap"

#define J9_GC_MANAGEMENT_POOL_NAME_TENURED				"tenured"
#define J9_GC_MANAGEMENT_POOL_NAME_TENURED_SOA			"tenured-SOA"
#define J9_GC_MANAGEMENT_POOL_NAME_TENURED_LOA			"tenured-LOA"
#define J9_GC_MANAGEMENT_POOL_NAME_NURSERY_ALLOCATE		"nursery-allocate"
#define J9_GC_MANAGEMENT_POOL_NAME_NURSERY_SURVIVOR		"nursery-survivor"

#define J9_GC_MANAGEMENT_POOL_NAME_BALANCED_RESERVED	"balanced-reserved"
#define J9_GC_MANAGEMENT_POOL_NAME_BALANCED_EDEN		"balanced-eden"
#define J9_GC_MANAGEMENT_POOL_NAME_BALANCED_SURVIVOR	"balanced-survivor"
#define J9_GC_MANAGEMENT_POOL_NAME_BALANCED_OLD		"balanced-old"

#define J9_GC_MANAGEMENT_GC_NAME_GLOBAL_OLD 			"MarkSweepCompact"
#define J9_GC_MANAGEMENT_GC_NAME_LOCAL_OLD			"Copy"
#define J9_GC_MANAGEMENT_GC_NAME_GLOBAL  			"global"
#define J9_GC_MANAGEMENT_GC_NAME_SCAVENGE 			"scavenge"
#define J9_GC_MANAGEMENT_GC_NAME_PGC 				"partial gc"
#define J9_GC_MANAGEMENT_GC_NAME_GGC 				"global garbage collect"
#define J9_GC_MANAGEMENT_GC_NAME_EPSILON 			"Epsilon"

UDATA j9gc_modron_global_collect(J9VMThread *vmThread);
UDATA j9gc_modron_local_collect(J9VMThread *vmThread);
UDATA j9gc_heap_total_memory(J9JavaVM *javaVM);
UDATA j9gc_heap_free_memory(J9JavaVM *javaVM);
UDATA j9gc_is_garbagecollection_disabled(J9JavaVM *javaVM);

UDATA j9gc_allsupported_memorypools(J9JavaVM *javaVM);
UDATA j9gc_allsupported_garbagecollectors(J9JavaVM *javaVM);
const char *j9gc_pool_name(J9JavaVM *javaVM, UDATA poolID);
const char *j9gc_garbagecollector_name(J9JavaVM *javaVM, UDATA gcID);
UDATA j9gc_is_managedpool_by_collector(J9JavaVM *javaVM, UDATA gcID, UDATA poolID);
UDATA j9gc_is_usagethreshold_supported(J9JavaVM *javaVM, UDATA poolID);
UDATA j9gc_is_collectionusagethreshold_supported(J9JavaVM *javaVM, UDATA poolID);
UDATA j9gc_is_local_collector(J9JavaVM *javaVM, UDATA gcID);
UDATA j9gc_get_collector_id(OMR_VMThread *omrVMThread);
UDATA j9gc_pools_memory(J9JavaVM *javaVM, UDATA poolIDs, UDATA *totals, UDATA *frees, BOOLEAN gcEnd);
UDATA j9gc_pool_maxmemory(J9JavaVM *javaVM, UDATA poolID);
const char *j9gc_get_gc_action(J9JavaVM *javaVM, UDATA gcID);
const char *j9gc_get_gc_cause(OMR_VMThread *omrVMthread);

UDATA j9gc_get_overflow_safe_alloc_size(J9JavaVM *javaVM);
UDATA j9gc_set_softmx(J9JavaVM *javaVM, UDATA newsoftMx);
UDATA j9gc_get_softmx(J9JavaVM *javaVM);
UDATA j9gc_get_initial_heap_size(J9JavaVM *javaVM);
UDATA j9gc_get_maximum_heap_size(J9JavaVM *javaVM);
const char *j9gc_get_gcmodestring(J9JavaVM *javaVM);
UDATA j9gc_get_object_size_in_bytes(J9JavaVM *javaVM, j9object_t objectPtr);
UDATA j9gc_get_object_total_footprint_in_bytes(J9JavaVM *javaVM, j9object_t objectPtr);
j9object_t j9gc_get_memoryController(J9VMThread *vmContext, j9object_t objectPtr);
void j9gc_set_memoryController(J9VMThread *vmThread, j9object_t objectPtr, j9object_t memoryController);
void j9gc_set_allocation_sampling_interval(J9VMThread *vmThread, UDATA samplingInterval);
void j9gc_set_allocation_threshold(J9VMThread *vmThread, UDATA low, UDATA high);
UDATA j9gc_get_bytes_allocated_by_thread(J9VMThread *vmThread);
void j9gc_get_CPU_times(J9JavaVM *javaVM, U_64 *masterCpuMillis, U_64 *slaveCpuMillis, U_32 *maxThreads, U_32 *currentThreads);
J9HookInterface** j9gc_get_private_hook_interface(J9JavaVM *javaVM);
/**
 * Called whenever a ownable synchronizer object is created. Places the object on the thread-specific buffer of recently allocated ownable synchronizer objects.
 * @param vmThread
 * @param object The object of type or subclass of java.util.concurrent.locks.AbstractOwnableSynchronizer.
 * @returns 0 if the object was successfully placed on the ownable synchronizer list
 */
UDATA ownableSynchronizerObjectCreated(J9VMThread *vmThread, j9object_t object);

/**
 * Called during class redefinition to notify the GC of replaced classes.In certain cases the GC needs to 
 * update some of the fields in the classes
 * This function must be called under exclusive access only
 * @param vmThread
 * @param originalClass The class being redifined
 * @param replacementClass The result of the the class redefinition
 * @param isFastHCR Flag to indicate wether it replacement was done via fastHCR or not
 */
void j9gc_notifyGCOfClassReplacement(J9VMThread *vmThread, J9Class *originalClass, J9Class *replacementClass, UDATA isFastHCR);
}

#endif /* MODRONAPI_HPP_ */
