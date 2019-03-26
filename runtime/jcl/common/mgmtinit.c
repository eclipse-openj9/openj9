/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

#include "jni.h"
#include "j9.h"
#include "mmhook.h"
#include "mmomrhook.h"
#include "jithook.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "j9modron.h"
#include "jcl_internal.h"

#include "mgmtinit.h"

/* required for memset */
#include <string.h>

#include "ut_j9jcl.h"

typedef enum {
	CLASS_MEMORY=0,
	MISC_MEMORY,
	JIT_CODECACHE,
	JIT_DATACACHE
} nonHeapMemoryPoolIndex;

static void managementClassLoadCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING))
static void managementClassUnloadCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
#endif
static void managementCompilingStartTime(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void managementCompilingEndTime(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void managementThreadStartCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void managementThreadEndCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

static VMINLINE void managementGC(OMR_VMThread *omrVMThread, void *userData, BOOLEAN isEnd);
static void managementGlobalGCStart(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void managementGlobalGCEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void managementLocalGCStart(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void managementLocalGCEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

static void managementCompactEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void gcStartEvent(J9JavaVM *vm, UDATA heapSize, UDATA heapUsed, UDATA *totals, UDATA *frees, UDATA collectorID);
static void gcEndEvent(J9JavaVM *vm, UDATA heapSize, UDATA heapUsed, UDATA *totals, UDATA *frees, UDATA *maxs, UDATA collectorID, OMR_VMThread *omrVMThread);
static jint initMemoryManagement(J9JavaVM *vm);
static U_32 getNumberSupported(U_32 supportedIDs);
static UDATA getArrayIndexFromManagerID(J9JavaLangManagementData *mgmt, UDATA id);
static void getSegmentSizes(J9JavaVM *javaVM, J9MemorySegmentList *segList, U_64 *storedSize, U_64 *storedUsed, U_64 *storedPeakSize, U_64 *storedPeakUsed);
static void updateNonHeapMemoryPoolSizes(J9JavaVM *vm, J9JavaLangManagementData *mgmt, BOOLEAN isGCEnd);

/* initialize java.lang.management data structures and hooks */
jint
managementInit(J9JavaVM *vm)
{
	J9HookInterface **vmHooks = NULL;
	J9HookInterface **omrGCHooks = NULL;
	J9VMThread *walkThread = NULL;
	J9JavaLangManagementData *mgmt = NULL;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	J9HookInterface **jitHooks = NULL;
#endif
	omrthread_t self = omrthread_self();
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* allocate management data struct */
	vm->managementData = j9mem_allocate_memory(sizeof(J9JavaLangManagementData), J9MEM_CATEGORY_VM_JCL);
	mgmt = vm->managementData;
	if (NULL == mgmt) {
		return JNI_ERR;
	}
	memset(mgmt, 0, sizeof(J9JavaLangManagementData));

	/* init monitor used to protect access to the U64 fields in the management data */
	if (omrthread_rwmutex_init(&mgmt->managementDataLock, 0, "management fields lock")) {
		return JNI_ERR;
	}

	if (JNI_ERR == initMemoryManagement(vm)) {
		return JNI_ERR;
	}

	/* init monitor used to wake up the heap threshold notification thread */
	if (omrthread_monitor_init(&mgmt->notificationMonitor, 0)) {
		return JNI_ERR;
	}

	mgmt->threadCpuTimeEnabledFlag = 1;
	if (0 <= omrthread_get_cpu_time(self)) {
		mgmt->isThreadCpuTimeSupported = 1;
	} else {
		mgmt->isThreadCpuTimeSupported = 0;
	}
	if (0 <= omrthread_get_self_cpu_time(self)) {
		mgmt->isCurrentThreadCpuTimeSupported = 1;
	} else {
		mgmt->isCurrentThreadCpuTimeSupported = 0;
	}
	/* isThreadCpuTimeSupported and isCurrentThreadCpuTimeSupported are read-only after this point */

	mgmt->vmStartTime = j9time_current_time_millis();

	/* vm->memoryManagerFunctions will be NULL if we failed to load the gc dll */
	if (NULL == mmFuncs) {
		return JNI_ERR;
	}

	mgmt->initialHeapSize = mmFuncs->j9gc_get_initial_heap_size(vm);
	mgmt->maximumHeapSize = mmFuncs->j9gc_get_maximum_heap_size(vm);

	/* hook class load and unload */
	vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INTERNAL_CLASS_LOAD, managementClassLoadCounter, OMR_GET_CALLSITE(), mgmt)) {
		return JNI_ERR;
	}

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, managementClassUnloadCounter, OMR_GET_CALLSITE(), mgmt)) {
		return JNI_ERR;
	}
#endif

	/* hook GC start/end events */
	omrGCHooks = mmFuncs->j9gc_get_omr_hook_interface(vm->omrVM);

	if ((*omrGCHooks)->J9HookRegisterWithCallSite(omrGCHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, managementGlobalGCStart, OMR_GET_CALLSITE(), vm)) {
		return JNI_ERR;
	}
	if ((*omrGCHooks)->J9HookRegisterWithCallSite(omrGCHooks, J9HOOK_MM_OMR_LOCAL_GC_START, managementLocalGCStart, OMR_GET_CALLSITE(), vm)) {
		return JNI_ERR;
	}
	if ((*omrGCHooks)->J9HookRegisterWithCallSite(omrGCHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, managementGlobalGCEnd, OMR_GET_CALLSITE(), vm)) {
		return JNI_ERR;
	}
	if ((*omrGCHooks)->J9HookRegisterWithCallSite(omrGCHooks, J9HOOK_MM_OMR_LOCAL_GC_END, managementLocalGCEnd, OMR_GET_CALLSITE(), vm)) {
		return JNI_ERR;
	}
	if ((*omrGCHooks)->J9HookRegisterWithCallSite(omrGCHooks, J9HOOK_MM_OMR_COMPACT_END, managementCompactEnd, OMR_GET_CALLSITE(), vm)) {
		return JNI_ERR;
	}

	/* lock the thread list */
	omrthread_monitor_enter(vm->vmThreadListMutex);

	/* count existing threads */
	walkThread = vm->mainThread;
	do {
		mgmt->liveJavaThreads++;
		if (walkThread->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) {
			mgmt->liveJavaDaemonThreads++;
		}
	} while((walkThread = walkThread->linkNext) != vm->mainThread);

	mgmt->totalJavaThreadsStarted = mgmt->liveJavaThreads;
	mgmt->peakLiveJavaThreads = mgmt->liveJavaThreads;

	/* hook thread create and destroy while holding the thread list mutex, so our initial counts stay correct */
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_CREATED, managementThreadStartCounter, OMR_GET_CALLSITE(), mgmt)) {
		omrthread_monitor_exit(vm->vmThreadListMutex);
		return JNI_ERR;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_DESTROY, managementThreadEndCounter, OMR_GET_CALLSITE(), mgmt)) {
		omrthread_monitor_exit(vm->vmThreadListMutex);
		return JNI_ERR;
	}

	omrthread_monitor_exit(vm->vmThreadListMutex);

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	/* hook jit compile start/end events */
	jitHooks = vm->internalVMFunctions->getJITHookInterface(vm);
	if (jitHooks) {
		if ((*jitHooks)->J9HookRegisterWithCallSite(jitHooks, J9HOOK_JIT_COMPILING_START, managementCompilingStartTime, OMR_GET_CALLSITE(), mgmt)) {
			return JNI_ERR;
		}
		if ((*jitHooks)->J9HookRegisterWithCallSite(jitHooks, J9HOOK_JIT_COMPILING_END, managementCompilingEndTime, OMR_GET_CALLSITE(), mgmt)) {
			return JNI_ERR;
		}
	}
#endif

	/* Initialization for OperatingSystemNotificationThread (mgmtos.c)
	 * OperatingSystemNotificationThread must be a singleton class.
	 */
	if (J9THREAD_SUCCESS != omrthread_monitor_init(&mgmt->dlparNotificationMonitor, 0)) {
		mgmt->dlparNotificationMonitor = NULL;
	}
	mgmt->dlparNotificationQueue = NULL;
	mgmt->dlparNotificationsPending = 0;
	mgmt->isCounterPathInitialized = 0;
	return 0;
}

static void
managementClassLoadCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9JavaLangManagementData *mgmt = userData;

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);

	mgmt->totalClassLoads++;

	omrthread_rwmutex_exit_write(mgmt->managementDataLock);
}


#if (defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING))

static void
managementClassUnloadCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMClassesUnloadEvent *data = eventData;
	J9JavaLangManagementData *mgmt = userData;

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);
	mgmt->totalClassUnloads += data->classUnloadCount;
	omrthread_rwmutex_exit_write(mgmt->managementDataLock);
}
#endif


static void 
managementCompilingStartTime(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9CompilingStartEvent *event = eventData;
	J9JavaVM *vm = event->currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JavaLangManagementData *mgmt = userData;
	I_64 now;

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);

	now = j9time_nano_time();
	if (0 < mgmt->threadsCompiling) {
		mgmt->totalCompilationTime += checkedTimeInterval((U_64)now, (U_64)mgmt->lastCompilationStart) * mgmt->threadsCompiling;
	}

	mgmt->lastCompilationStart = now;
	mgmt->threadsCompiling++;

	omrthread_rwmutex_exit_write(mgmt->managementDataLock);
}
static void 
managementCompilingEndTime(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9CompilingEndEvent *event = eventData;
	J9JavaVM* vm = event->currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JavaLangManagementData *mgmt = userData;

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);

	mgmt->totalCompilationTime += checkedTimeInterval((U_64)j9time_nano_time(), (U_64)mgmt->lastCompilationStart);
	mgmt->threadsCompiling--;

	omrthread_rwmutex_exit_write(mgmt->managementDataLock);
}
/* tear down java.lang.management data structures and hooks */
void
managementTerminate(J9JavaVM *vm)
{
	J9HookInterface **vmHooks = NULL;
	J9HookInterface **omrGCHooks = NULL;
	J9JavaLangManagementData *mgmt = vm->managementData;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	J9HookInterface **jitHooks;
#endif
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (NULL == mgmt) {
		/* managementInit was never called, we have nothing to clean up */
		return;
	}

	/* unhook class load and unload */
	vmHooks = vm->internalVMFunctions->getVMHookInterface(vm);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_INTERNAL_CLASS_LOAD, managementClassLoadCounter, mgmt);

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, managementClassUnloadCounter, mgmt);
#endif

	
	/* vm->memoryManagerFunctions will be NULL if we failed to load the gc dll */
	if (NULL != mmFuncs) {
		/* unhook GC start/end */
		omrGCHooks = mmFuncs->j9gc_get_omr_hook_interface(vm->omrVM);

		(*omrGCHooks)->J9HookUnregister(omrGCHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, managementGlobalGCStart, vm);
		(*omrGCHooks)->J9HookUnregister(omrGCHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, managementGlobalGCEnd, vm);
		(*omrGCHooks)->J9HookUnregister(omrGCHooks, J9HOOK_MM_OMR_LOCAL_GC_START, managementLocalGCStart, vm);
		(*omrGCHooks)->J9HookUnregister(omrGCHooks, J9HOOK_MM_OMR_LOCAL_GC_END, managementLocalGCEnd, vm);
		(*omrGCHooks)->J9HookUnregister(omrGCHooks, J9HOOK_MM_OMR_COMPACT_END, managementCompactEnd, vm);
	}

	/* unhook thread start and end */
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_CREATED, managementThreadStartCounter, mgmt);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_DESTROY, managementThreadEndCounter, mgmt);

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	/* unhook jit compile start/end events */
	jitHooks = vm->internalVMFunctions->getJITHookInterface(vm);
	if (jitHooks) {
		(*jitHooks)->J9HookUnregister(jitHooks, J9HOOK_JIT_COMPILING_START, managementCompilingStartTime, mgmt);
		(*jitHooks)->J9HookUnregister(jitHooks, J9HOOK_JIT_COMPILING_END, managementCompilingEndTime, mgmt);
	}
#endif

	/* destroy monitor */
	omrthread_rwmutex_destroy(mgmt->managementDataLock);

	omrthread_monitor_destroy(mgmt->notificationMonitor);

	/* Cleanup for OperatingSystemNotificationThread (mgmtos.c) */
	if (NULL != mgmt->dlparNotificationMonitor) {
		omrthread_monitor_destroy(mgmt->dlparNotificationMonitor);
	}
	
	/* deallocate management data struct */
	j9mem_free_memory(mgmt->memoryPools);
	j9mem_free_memory(mgmt->garbageCollectors);
	j9mem_free_memory(mgmt->nonHeapMemoryPools);
	j9mem_free_memory(vm->managementData);
}

static void
managementThreadStartCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9JavaLangManagementData *mgmt = userData;
	J9VMThreadCreatedEvent *event = (J9VMThreadCreatedEvent *)eventData;
	J9VMThread *vmThread = event->vmThread;

	if (NULL != vmThread) {
		omrthread_rwmutex_enter_write(mgmt->managementDataLock);

		mgmt->totalJavaThreadsStarted++;
		mgmt->liveJavaThreads++;
		if (mgmt->liveJavaThreads > mgmt->peakLiveJavaThreads) {
			mgmt->peakLiveJavaThreads = mgmt->liveJavaThreads;
		}
		if (J9_PRIVATE_FLAGS_DAEMON_THREAD & vmThread->privateFlags) {
			mgmt->liveJavaDaemonThreads++;
		}

		omrthread_rwmutex_exit_write(mgmt->managementDataLock);
	}
}
static void
managementThreadEndCounter(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9JavaLangManagementData *mgmt = userData;
	J9VMThreadDestroyEvent *event = (J9VMThreadDestroyEvent *)eventData;
	J9VMThread *vmThread = event->vmThread;

	if (NULL != vmThread) {
		omrthread_rwmutex_enter_write(mgmt->managementDataLock);

		mgmt->liveJavaThreads--;
		if (J9_PRIVATE_FLAGS_DAEMON_THREAD & vmThread->privateFlags) {
			mgmt->liveJavaDaemonThreads--;
		}

		omrthread_rwmutex_exit_write(mgmt->managementDataLock);
	}
}

static void
managementGC(OMR_VMThread *omrVMThread, void *userData, BOOLEAN isEnd)
{
	J9JavaVM *vm = userData;
	UDATA heapSize = 0;
	UDATA heapFree = 0;
	UDATA heapUsed = 0;
	UDATA totals[J9VM_MAX_HEAP_MEMORYPOOL_COUNT];
	UDATA frees[J9VM_MAX_HEAP_MEMORYPOOL_COUNT];
	UDATA maxs[J9VM_MAX_HEAP_MEMORYPOOL_COUNT];
	UDATA collectorID = 0;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	U_32 supportedMemoryPoolIDs = 0;
	U_32 id = 0;
	U_32 mask = 0;
	UDATA idx = 0;
	UDATA count = 0;

	collectorID = mmFuncs->j9gc_get_collector_id(omrVMThread);

	if (0 == collectorID) {
		/* only record data for "stop the world" collector */
		return;
	}

	heapSize = mmFuncs->j9gc_heap_total_memory(vm);
	heapFree = mmFuncs->j9gc_heap_free_memory(vm);
	heapUsed = heapSize - heapFree;

	mmFuncs->j9gc_pools_memory(vm, 0, &totals[0], &frees[0], isEnd);
	if (isEnd) {
		supportedMemoryPoolIDs = (U_32)mmFuncs->j9gc_allsupported_memorypools(vm);
		for (count = 0, mask = 1, idx = 0; count < J9_GC_MANAGEMENT_MAX_POOL; ++count, mask <<= 1) {
			id = (supportedMemoryPoolIDs & mask);
			if (0 != id) {
				maxs[idx] = mmFuncs->j9gc_pool_maxmemory(vm, id);
				idx += 1;
			}
		}
		gcEndEvent(vm, heapSize, heapUsed, &totals[0], &frees[0], &maxs[0], collectorID, omrVMThread);
	} else {
		gcStartEvent(vm, heapSize, heapUsed, &totals[0], &frees[0], collectorID);
	}
}


static void
managementGlobalGCStart(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GlobalGCStartEvent *event = (MM_GlobalGCStartEvent *)eventData;
	managementGC(event->currentThread, userData, FALSE);
}
static void managementGlobalGCEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GlobalGCEndEvent *event = (MM_GlobalGCEndEvent *)eventData;
	managementGC(event->currentThread, userData, TRUE);
}
static void managementLocalGCStart(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_LocalGCStartEvent *event = (MM_LocalGCStartEvent*)eventData;
	managementGC(event->currentThread, userData, FALSE);
}
static void managementLocalGCEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_LocalGCEndEvent *event = (MM_LocalGCEndEvent*)eventData;
	managementGC(event->currentThread, userData, TRUE);
}

static void
managementCompactEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9JavaVM *vm = userData;
	J9JavaLangManagementData *mgmt = vm->managementData;
	UDATA idx = 0;

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);

	for (idx = 0; idx < (UDATA)mgmt->supportedCollectors; ++idx) {
		if (0 == (mgmt->garbageCollectors[idx].id & J9VM_MANAGEMENT_GC_LOCAL)) {
			mgmt->garbageCollectors[idx].totalCompacts += 1;
			break;
		}
	}

	omrthread_rwmutex_exit_write(mgmt->managementDataLock);
}

/* Updates java.lang.management data for the start of a GC. */
static void
gcStartEvent(J9JavaVM *vm, UDATA heapSize, UDATA heapUsed, UDATA *totals, UDATA *frees, UDATA collectorID)
{
	J9JavaLangManagementData *mgmt = vm->managementData;
	J9GarbageCollectorData *gcData = &mgmt->garbageCollectors[getArrayIndexFromManagerID(mgmt, collectorID)];
	J9MemoryPoolData *memoryPools = mgmt->memoryPools;
	J9MemoryNotification *notification = NULL;
	J9MemoryNotification *last = NULL;

	UDATA total = 0;
	UDATA used = 0;
	UDATA idx = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);
	/* lock the management struct */
	omrthread_rwmutex_enter_write(mgmt->managementDataLock);

	/* check the wall clock */
	/* the start time in milliseconds since the Java virtual machine was started */
	gcData->lastGcInfo.startTime = j9time_current_time_millis();
	if (gcData->lastGcInfo.startTime < (U_64)mgmt->vmStartTime) {
		/* startTime is earlier than vmStartTime in case of wall clock correction while interval is measuring */
		gcData->lastGcInfo.startTime = 0;
	} else {
		gcData->lastGcInfo.startTime -= mgmt->vmStartTime;
	}

	mgmt->preCollectionHeapSize = heapSize;
	mgmt->preCollectionHeapUsed = heapUsed;

	for (idx = 0; idx < mgmt->supportedMemoryPools; ++idx) {
		J9MemoryPoolData *memoryPool = &memoryPools[idx];
		total = totals[idx];
		used = totals[idx] - frees[idx];

		memoryPool->preCollectionSize = total;
		memoryPool->preCollectionUsed = used;
		memoryPool->preCollectionMaxSize = memoryPool->postCollectionMaxSize;

		/* check the peak usage and update */
		if (memoryPool->peakUsed < used) {
			memoryPool->peakUsed = used;
			memoryPool->peakSize = total;
			memoryPool->peakMax = memoryPool->preCollectionMaxSize;
		}

		/* if a heap usage threshold is set, check whether we are above or below */
		if (0 < memoryPool->usageThreshold) {
			if (memoryPool->usageThreshold < used) {
				/* usage above threshold now, was it below threshold last time? */
				if (0 == (memoryPool->notificationState & THRESHOLD_EXCEEDED)) {

					/* if so, generate a notification and set the flag */
					memoryPool->notificationState |= THRESHOLD_EXCEEDED;
					memoryPool->usageThresholdCrossedCount++;

					/* if the queue is full, silently discard this notification */
					if (NOTIFICATION_QUEUE_MAX > mgmt->notificationsPending) {
						notification = j9mem_allocate_memory(sizeof(*notification), J9MEM_CATEGORY_VM_JCL);
						if (NULL != notification) {
							notification->gcInfo = NULL;
							notification->usageThreshold = j9mem_allocate_memory(sizeof(*notification->usageThreshold), J9MEM_CATEGORY_VM_JCL);
							if (NULL == notification->usageThreshold) {
								j9mem_free_memory(notification);
								notification = NULL;
							}
						}
						/* in case of allocation failure, silently discard the notification */
						if (NULL != notification) {
							/* populate the notification data */
							notification->type = THRESHOLD_EXCEEDED;
							notification->next = NULL;
							notification->usageThreshold->poolID = memoryPool->id;
							notification->usageThreshold->usedSize = used;
							notification->usageThreshold->totalSize = total;
							notification->usageThreshold->maxSize = memoryPool->preCollectionMaxSize;
							notification->usageThreshold->thresholdCrossingCount = memoryPool->usageThresholdCrossedCount;
							notification->sequenceNumber = mgmt->notificationCount++;

							/* notify the thread that dispatches notifications to Java handlers */
							omrthread_monitor_enter(mgmt->notificationMonitor);

							/* find the end of the queue and count the entries */
							last = mgmt->notificationQueue;
							while ((last != NULL) && (last->next != NULL)) {
								last = last->next;
							}
							/* add to queue */
							if (NULL == last) {
								mgmt->notificationQueue = notification;
								last = notification;
							} else {
								last->next = notification;
							}
							mgmt->notificationsPending += 1;

							omrthread_monitor_notify(mgmt->notificationMonitor);
							omrthread_monitor_exit(mgmt->notificationMonitor);
						}
					}
				}
			} else {
				/* usage below threshold now, was it above threshold last time? */
				if (0 != (memoryPool->notificationState & THRESHOLD_EXCEEDED)) {
					/* if so, clear the flag */
					memoryPool->notificationState &= ~THRESHOLD_EXCEEDED;
				}
			}
		}
	}

	omrthread_rwmutex_exit_write(mgmt->managementDataLock);

	/* update nonHeap memory pools for preCollection */
	updateNonHeapMemoryPoolSizes(vm, mgmt, FALSE);
}

/* Updates java.lang.management data for the end of a GC. */
static void
gcEndEvent(J9JavaVM *vm, UDATA heapSize, UDATA heapUsed, UDATA *totals, UDATA *frees, UDATA *maxs, UDATA collectorID, OMR_VMThread *omrVMThread)
{
	J9JavaLangManagementData *mgmt = vm->managementData;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	J9GarbageCollectorData *gcData = &mgmt->garbageCollectors[getArrayIndexFromManagerID(mgmt, collectorID)];
	J9MemoryPoolData *memoryPools = mgmt->memoryPools;
	J9MemoryNotification *notification = NULL;
	J9MemoryNotification *last = NULL;

	UDATA total = 0;
	UDATA used = 0;
	UDATA idx = 0;
	U_32 notificationEnabled = 0;

	UDATA supportedMemoryPools = mgmt->supportedMemoryPools;
	UDATA supportedNonHeapMemoryPools = mgmt->supportedNonHeapMemoryPools;
	J9GarbageCollectionInfo* gcInfo = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* lock the management struct */
	omrthread_rwmutex_enter_write(mgmt->managementDataLock);

	mgmt->postCollectionHeapSize = heapSize;
	mgmt->postCollectionHeapUsed = heapUsed;

	for (idx = 0; idx < mgmt->supportedMemoryPools; ++idx) {
		J9MemoryPoolData *memoryPool = &memoryPools[idx];
		total = totals[idx];
		used = totals[idx] - frees[idx];

		memoryPool->postCollectionSize = total;
		memoryPool->postCollectionUsed = used;
		memoryPool->postCollectionMaxSize = maxs[idx];

		/* check the peak usage and update */
		if (memoryPool->peakUsed < used) {
			memoryPool->peakUsed = used;
			memoryPool->peakSize = total;
			memoryPool->peakMax = memoryPool->postCollectionMaxSize;
		}

		/* if a heap collection threshold is set, check whether we are above or below */
		if (0 < memoryPool->collectionUsageThreshold) {
			if (memoryPool->collectionUsageThreshold < used) {
				/* usage above threshold now, was it below threshold last time? */
				if (0 == (memoryPool->notificationState & COLLECTION_THRESHOLD_EXCEEDED)) {

					/* if so, generate a notification and set the flag */
					memoryPool->notificationState |= COLLECTION_THRESHOLD_EXCEEDED;
					memoryPool->collectionUsageThresholdCrossedCount++;

					/* if the queue is full, silently discard this notification */
					if (NOTIFICATION_QUEUE_MAX > mgmt->notificationsPending) {
						notification = j9mem_allocate_memory(sizeof(*notification), J9MEM_CATEGORY_VM_JCL);
						/* in case of allocation failure, silently discard the notification */
						if (NULL != notification) {
							notification->gcInfo = NULL;
							notification->usageThreshold = j9mem_allocate_memory(sizeof(*notification->usageThreshold), J9MEM_CATEGORY_VM_JCL);
							if (NULL == notification->usageThreshold) {
								j9mem_free_memory(notification);
								notification = NULL;
							}
						}
						if (NULL != notification) {
							/* populate the notification data */
							notification->type = COLLECTION_THRESHOLD_EXCEEDED;
							notification->next = NULL;
							notification->usageThreshold->poolID = memoryPool->id;
							notification->usageThreshold->usedSize = used;
							notification->usageThreshold->totalSize = total;
							notification->usageThreshold->maxSize = memoryPool->postCollectionMaxSize;
							notification->usageThreshold->thresholdCrossingCount = memoryPool->collectionUsageThresholdCrossedCount;
							notification->sequenceNumber = mgmt->notificationCount++;

							/* notify the thread that dispatches notifications to Java handlers */
							omrthread_monitor_enter(mgmt->notificationMonitor);
							/* find the end of the queue and count the entries */
							last = mgmt->notificationQueue;
							/* find the end of the queue and count the entries */
							while ((NULL != last) && (NULL != last->next)) {
								last = last->next;
							}
							/* add to queue */
							if (NULL == last) {
								mgmt->notificationQueue = notification;
								last = notification;
							} else {
								last->next = notification;
							}

							mgmt->notificationsPending += 1;
							omrthread_monitor_notify(mgmt->notificationMonitor);
							omrthread_monitor_exit(mgmt->notificationMonitor);
						}
					}
				}
			} else {
				/* usage below threshold now, was it above threshold last time? */
				if (0 != (memoryPool->notificationState & COLLECTION_THRESHOLD_EXCEEDED)) {
					/* if so, clear the flag */
					memoryPool->notificationState &= ~COLLECTION_THRESHOLD_EXCEEDED;
				}
			}
		}
	}

	/* update the relevant stats */
	/* the end time in milliseconds since the Java virtual machine was started */
	gcData->lastGcInfo.endTime = j9time_current_time_millis();
	if (gcData->lastGcInfo.endTime < (U_64)mgmt->vmStartTime) {
		/* endTime is earlier than vmStartTime in case of wall clock correction while interval is measuring */
		gcData->lastGcInfo.endTime = 0;
	} else {
		gcData->lastGcInfo.endTime -= mgmt->vmStartTime;
	}
	/* endTime is earlier than startTime in case of wall clock correction while interval is measuring */
	if (gcData->lastGcInfo.endTime >= gcData->lastGcInfo.startTime) {
		gcData->totalGCTime += (gcData->lastGcInfo.endTime - gcData->lastGcInfo.startTime);
	} else {
		gcData->lastGcInfo.endTime = gcData->lastGcInfo.startTime;
	}

	/* collectionCount */
	gcData->lastGcInfo.index += 1;
	gcData->memoryUsed = 0;

	for (idx = 0; idx < mgmt->supportedMemoryPools; ++idx) {
		if (0 != mmFuncs->j9gc_is_managedpool_by_collector(vm, (gcData->id & J9VM_MANAGEMENT_GC_HEAP_ID_MASK), (memoryPools[idx].id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK))) {
			gcData->memoryUsed += memoryPools[idx].postCollectionUsed;
		}
	}
	
	gcData->totalMemoryFreed += (I_64)(mgmt->preCollectionHeapUsed - mgmt->postCollectionHeapUsed);
	
	/* update the GC CPU usage */
	mmFuncs->j9gc_get_CPU_times(vm, &mgmt->gcMasterCpuTime, &mgmt->gcSlaveCpuTime, &mgmt->gcMaxThreads, &mgmt->gcCurrentThreads);
	
	/* update nonHeap memory pools for postCollection */
	updateNonHeapMemoryPoolSizes(vm, mgmt, TRUE);
	/* update J9GarbageCollectionInfo for the collector */
	gcInfo = &gcData->lastGcInfo;

	gcInfo->gcID = gcData->id;
	gcInfo->gcAction = mmFuncs->j9gc_get_gc_action(vm, (gcInfo->gcID & J9VM_MANAGEMENT_GC_HEAP_ID_MASK));
	gcInfo->gcCause = mmFuncs->j9gc_get_gc_cause(omrVMThread);
	gcInfo->arraySize =(U_32) (supportedMemoryPools + supportedNonHeapMemoryPools);
	/* heap memory pools */
	for (idx = 0; supportedMemoryPools > idx; ++idx) {
		J9MemoryPoolData *memoryPool = &memoryPools[idx];
		gcInfo->initialSize[idx] = memoryPool->initialSize;
		gcInfo->preUsed[idx] = memoryPool->preCollectionUsed;
		gcInfo->preCommitted[idx] = memoryPool->preCollectionSize;
		gcInfo->preMax[idx] = (I_64) memoryPool->preCollectionMaxSize;
		gcInfo->postUsed[idx] = memoryPool->postCollectionUsed;
		gcInfo->postCommitted[idx] = memoryPool->postCollectionSize;
		gcInfo->postMax[idx] = (I_64) memoryPool->postCollectionMaxSize;
	}
	/* non heap memory pools */
	for (; supportedMemoryPools + supportedNonHeapMemoryPools > idx; ++idx) {
		J9NonHeapMemoryData *nonHeapMemory = &mgmt->nonHeapMemoryPools[idx-supportedMemoryPools];
		gcInfo->initialSize[idx] = nonHeapMemory->initialSize;
		gcInfo->preUsed[idx] = nonHeapMemory->preCollectionUsed;
		gcInfo->preCommitted[idx] = nonHeapMemory->preCollectionSize;
		gcInfo->preMax[idx] = nonHeapMemory->maxSize;
		gcInfo->postUsed[idx] = nonHeapMemory->postCollectionUsed;
		gcInfo->postCommitted[idx] = nonHeapMemory->postCollectionSize;
		gcInfo->postMax[idx] = nonHeapMemory->maxSize;
	}

	/* garbage collection notification */
	notificationEnabled = mgmt->notificationEnabled;
	omrthread_rwmutex_exit_write(mgmt->managementDataLock);

	if (0 !=notificationEnabled) {
		notification = j9mem_allocate_memory(sizeof(*notification), J9MEM_CATEGORY_VM_JCL);
		/* in case of allocation failure, silently discard the notification */
		if (NULL != notification) {
			notification->usageThreshold = NULL;
			notification->gcInfo = j9mem_allocate_memory(sizeof(*notification->gcInfo), J9MEM_CATEGORY_VM_JCL);
			if (NULL == notification->gcInfo) {
				j9mem_free_memory(notification);
				notification = NULL;
			}
		}
		if (NULL != notification) {
			memcpy(notification->gcInfo, gcInfo, sizeof(*notification->gcInfo));

			notification->type = END_OF_GARBAGE_COLLECTION;
			notification->next = NULL;
			notification->sequenceNumber = mgmt->notificationCount++;

			/* notify the thread that dispatches notifications to Java handlers */
			omrthread_monitor_enter(mgmt->notificationMonitor);
			/* find the end of the queue and count the entries */
			last = mgmt->notificationQueue;
			/* find the end of the queue and count the entries */
			while ((NULL != last) && (NULL != last->next)) {
				last = last->next;
			}
			/* add to queue */
			if (NULL == last) {
				mgmt->notificationQueue = notification;
				last = notification;
			} else {
				last->next = notification;
			}

			mgmt->notificationsPending += 1;
			omrthread_monitor_notify(mgmt->notificationMonitor);
			omrthread_monitor_exit(mgmt->notificationMonitor);
		}
	}
}

static jint
initMemoryManagement(J9JavaVM *vm)
{
	J9JavaLangManagementData *mgmt = vm->managementData;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	U_32 supportedMemoryPoolIDs = (U_32)mmFuncs->j9gc_allsupported_memorypools(vm);
	U_32 supportedCollectorIDs  = (U_32)mmFuncs->j9gc_allsupported_garbagecollectors(vm);
	U_32 id = 0;
	U_32 mask = 0;
	UDATA idx = 0;
	UDATA count = 0;
	UDATA totals[J9VM_MAX_HEAP_MEMORYPOOL_COUNT];
	UDATA frees[J9VM_MAX_HEAP_MEMORYPOOL_COUNT];
 	U_64 used = 0;
	J9MemorySegmentList *segList = NULL;

	PORT_ACCESS_FROM_JAVAVM(vm);
	mgmt->supportedMemoryPools = getNumberSupported(supportedMemoryPoolIDs);
	mgmt->supportedCollectors = getNumberSupported(supportedCollectorIDs);

	/* initialize Heap memory pools (initSize, maxSize) */
	mgmt->memoryPools = j9mem_allocate_memory((sizeof(*mgmt->memoryPools) * mgmt->supportedMemoryPools), J9MEM_CATEGORY_VM_JCL);
	if (NULL == mgmt->memoryPools) {
		return JNI_ERR;
	}
	memset(mgmt->memoryPools, 0, (sizeof(*mgmt->memoryPools) * mgmt->supportedMemoryPools));
	/* request all supported pool data for initial size */
	mmFuncs->j9gc_pools_memory(vm, 0, &totals[0], &frees[0], FALSE);

	for (count = 0, mask = 1, idx = 0; count < J9_GC_MANAGEMENT_MAX_POOL; ++count, mask <<= 1) {

		id = (supportedMemoryPoolIDs & mask);
		if (0 != id) {
			strcpy(mgmt->memoryPools[idx].name, mmFuncs->j9gc_pool_name(vm, id));
			mgmt->memoryPools[idx].id = (U_32) (id | J9VM_MANAGEMENT_POOL_HEAP);
			mgmt->memoryPools[idx].initialSize = totals[idx];
			mgmt->memoryPools[idx].preCollectionMaxSize = mmFuncs->j9gc_pool_maxmemory(vm, id);
			mgmt->memoryPools[idx].postCollectionMaxSize = mgmt->memoryPools[idx].preCollectionMaxSize;
			idx += 1;
		}
	}

	/* initialize Garbage Collectors */
	mgmt->garbageCollectors = j9mem_allocate_memory((sizeof(*mgmt->garbageCollectors) * mgmt->supportedCollectors), J9MEM_CATEGORY_VM_JCL);
	if (NULL == mgmt->garbageCollectors) {
		return JNI_ERR;
	}
	memset(mgmt->garbageCollectors, 0, (sizeof(*mgmt->garbageCollectors) * mgmt->supportedCollectors));

	for (count = 0, mask = 1, idx = 0; count < J9_GC_MANAGEMENT_MAX_COLLECTOR; ++count, mask <<= 1) {

		id = supportedCollectorIDs&mask;
		if (0 != id) {
			strcpy(mgmt->garbageCollectors[idx].name, mmFuncs->j9gc_garbagecollector_name(vm, id));
			mgmt->garbageCollectors[idx].id = (U_32) (id | J9VM_MANAGEMENT_GC_HEAP);
			if (mmFuncs->j9gc_is_local_collector(vm, id)) {
				mgmt->garbageCollectors[idx].id |= J9VM_MANAGEMENT_GC_LOCAL;
			}
			idx += 1;
		}
	}

	/* initialize nonHeap memory pools  (initSize) */
	mgmt->supportedNonHeapMemoryPools = 2; /* "class storage", "miscellaneous non-heap storage" */
#if defined( J9VM_INTERP_NATIVE_SUPPORT )
	if (vm->jitConfig) {
		mgmt->supportedNonHeapMemoryPools += 2; /* "JIT code cache", "JIT data cache" */
	}
#endif
	mgmt->nonHeapMemoryPools = j9mem_allocate_memory((sizeof(*mgmt->nonHeapMemoryPools) * mgmt->supportedNonHeapMemoryPools), J9MEM_CATEGORY_VM_JCL);
	if (NULL == mgmt->nonHeapMemoryPools) {
		return JNI_ERR;
	}
	memset(mgmt->nonHeapMemoryPools, 0, (sizeof(*mgmt->nonHeapMemoryPools) * mgmt->supportedNonHeapMemoryPools));

	for (idx = 0; idx < mgmt->supportedNonHeapMemoryPools; idx++) {
		switch (idx) {
		case CLASS_MEMORY:
			mgmt->nonHeapMemoryPools[idx].id = J9VM_MANAGEMENT_POOL_NONHEAP_SEG_CLASSES;
			strcpy(mgmt->nonHeapMemoryPools[idx].name, J9VM_MANAGEMENT_NONHEAPPOOL_NAME_CLASSES);
			segList = vm->classMemorySegments;
			mgmt->nonHeapMemoryPools[idx].maxSize = -1;
			break;
		case MISC_MEMORY:
			mgmt->nonHeapMemoryPools[idx].id = J9VM_MANAGEMENT_POOL_NONHEAP_SEG_MISC;
			strcpy(mgmt->nonHeapMemoryPools[idx].name, J9VM_MANAGEMENT_NONHEAPPOOL_NAME_MISC);
			segList = vm->memorySegments;
			mgmt->nonHeapMemoryPools[idx].maxSize = -1;
			break;
		case JIT_CODECACHE:
			mgmt->nonHeapMemoryPools[idx].id = J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_CODE;
			strcpy(mgmt->nonHeapMemoryPools[idx].name, J9VM_MANAGEMENT_NONHEAPPOOL_NAME_JITCODE);
			segList = vm->jitConfig->codeCacheList;
			mgmt->nonHeapMemoryPools[idx].maxSize = vm->jitConfig->codeCacheTotalKB * 1024;
			break;
		case JIT_DATACACHE:
			mgmt->nonHeapMemoryPools[idx].id = J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_DATA;
			strcpy(mgmt->nonHeapMemoryPools[idx].name, J9VM_MANAGEMENT_NONHEAPPOOL_NAME_JITDATA);
			segList = vm->jitConfig->dataCacheList;
			mgmt->nonHeapMemoryPools[idx].maxSize = vm->jitConfig->dataCacheTotalKB * 1024;
			break;
		default:
			/* Unreachable */
			Assert_JCL_unreachable();
		}
		getSegmentSizes(vm, segList, &mgmt->nonHeapMemoryPools[idx].initialSize, &used, &mgmt->nonHeapMemoryPools[idx].peakSize, &mgmt->nonHeapMemoryPools[idx].peakUsed);
	}
	return 0;

}

static U_32
getNumberSupported(U_32 supportedIDs)
{
    U_32 numSupported = 0;
    U_32 ids = supportedIDs;
    while (0 != ids) {
        ids &= ids - 1; /* clear the least bit set */
        numSupported += 1;
    }
    return numSupported;
}

static UDATA
getArrayIndexFromManagerID(J9JavaLangManagementData *mgmt, UDATA id)
{
	UDATA idx = 0;

	for(; idx < mgmt->supportedCollectors; ++idx) {
		if ((J9VM_MANAGEMENT_GC_HEAP_ID_MASK & mgmt->garbageCollectors[idx].id) == (J9VM_MANAGEMENT_GC_HEAP_ID_MASK & id)) {
			return idx;
		}
	}
	return 0;
}

static void
getSegmentSizes(J9JavaVM *javaVM, J9MemorySegmentList *segList, U_64 *storedSize, U_64 *storedUsed, U_64 *storedPeakSize, U_64 *storedPeakUsed)
{
	U_64 used = 0;
	U_64 committed = 0;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_monitor_enter(segList->segmentMutex);

	MEMORY_SEGMENT_LIST_DO(segList, seg)
		used += seg->heapAlloc - seg->heapBase;
		committed += seg->size;
	END_MEMORY_SEGMENT_LIST_DO(seg)

	omrthread_monitor_exit(segList->segmentMutex);

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);
	*storedSize = committed;
	*storedUsed = used;
	if (used > *storedPeakUsed) {
		*storedPeakUsed = used;
		*storedPeakSize = committed;
	}
	omrthread_rwmutex_exit_write(mgmt->managementDataLock);
}

static void
updateNonHeapMemoryPoolSizes(J9JavaVM *vm, J9JavaLangManagementData *mgmt, BOOLEAN isGCEND)
{
	U_32 idx = 0;
	U_64 *storedSize = NULL;
	U_64 *storedUsed = NULL;
	J9MemorySegmentList *segList = NULL;
	
	for (; idx < mgmt->supportedNonHeapMemoryPools; ++idx) {
		switch (idx) {
		case CLASS_MEMORY:
			segList = vm->classMemorySegments;
			break;
		case MISC_MEMORY:
			segList = vm->memorySegments;
			break;
		case JIT_CODECACHE:
			segList = vm->jitConfig->codeCacheList;
			break;
		case JIT_DATACACHE:
			segList = vm->jitConfig->dataCacheList;
			break;
		default:
			/* Unreachable */
			Assert_JCL_unreachable();
		}
		if (isGCEND) {
			storedSize = &mgmt->nonHeapMemoryPools[idx].postCollectionSize;
			storedUsed = &mgmt->nonHeapMemoryPools[idx].postCollectionUsed;
		} else {
			storedSize = &mgmt->nonHeapMemoryPools[idx].preCollectionSize;
			storedUsed = &mgmt->nonHeapMemoryPools[idx].preCollectionUsed;
		}
		getSegmentSizes(vm, segList, storedSize, storedUsed, &mgmt->nonHeapMemoryPools[idx].peakSize, &mgmt->nonHeapMemoryPools[idx].peakUsed);
	}
}
