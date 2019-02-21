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
#include "mgmtinit.h"
#include "j9modron.h"

static jobject processSegmentList(JNIEnv *env, jclass memoryUsage, jobject memUsageConstructor, J9MemorySegmentList *segList, U_64 initSize, I_64 maxSize, U_64 *storedPeakSize, U_64 *storedPeakUsed, UDATA action);
static UDATA getIndexFromPoolID(J9JavaLangManagementData *mgmt, UDATA id);
static J9MemorySegmentList *getMemorySegmentList(J9JavaVM *javaVM, jint id);

jobject JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getCollectionUsageImpl(JNIEnv *env, jobject beanInstance, jint id, jclass memoryUsage, jobject memUsageConstructor)
{
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		jlong used = 0;
		jlong committed = 0;
		jlong init = 0;
		jlong max = 0;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		jmethodID ctor = NULL;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		used = (jlong) pool->postCollectionUsed;
		committed = (jlong) pool->postCollectionSize;
		init = (jlong) pool->initialSize;
		max = (jlong) pool->postCollectionMaxSize;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);

		ctor = (*env)->FromReflectedMethod(env, memUsageConstructor);
		if(NULL == ctor) {
			return NULL;
		}

		return (*env)->NewObject(env, memoryUsage, ctor, init, used, committed, max);
	} else {
		return NULL;
	}
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getCollectionUsageThresholdImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jlong count = -1;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData* pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		count = (jlong) pool->collectionUsageThreshold;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
	}
	return count;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getCollectionUsageThresholdCountImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jlong count = -1;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

		omrthread_rwmutex_enter_read( mgmt->managementDataLock );
		count = (jlong) pool->collectionUsageThresholdCrossedCount;
		omrthread_rwmutex_exit_read( mgmt->managementDataLock );
	}
	return count;
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getPeakUsageImpl(JNIEnv *env, jobject beanInstance, jint id, jclass memoryUsage, jobject memUsageConstructor)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		/* Heap MemoryPool */
		jlong used = 0;
		jlong committed = 0;
		jlong init = 0;
		jlong max = 0;
		jmethodID ctor = NULL;
		UDATA idx = getIndexFromPoolID(mgmt, id);
		J9MemoryPoolData *pool = &mgmt->memoryPools[idx];
		UDATA currentUsed = 0;
		UDATA currentCommitted = 0;
		UDATA totals[J9_GC_MANAGEMENT_MAX_POOL];
		UDATA frees[J9_GC_MANAGEMENT_MAX_POOL];

		javaVM->memoryManagerFunctions->j9gc_pools_memory(javaVM, (id&J9VM_MANAGEMENT_POOL_HEAP_ID_MASK), &totals[0], &frees[0], FALSE);
		currentCommitted = totals[idx];
		currentUsed = totals[idx] - frees[idx];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		used = (jlong) pool->peakUsed;
		committed = (jlong) pool->peakSize;
		init = (jlong) pool->initialSize;
		max = (jlong) pool->peakMax;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);

		if (currentUsed > (UDATA)used) {
			/* the current usage is above the recorded peak but no one else has noticed yet */
			omrthread_rwmutex_enter_write(mgmt->managementDataLock);
			if (currentUsed > pool->peakUsed) {
				pool->peakUsed = currentUsed;
				pool->peakSize = currentCommitted;
				pool->peakMax = pool->postCollectionMaxSize;
				used = currentUsed;
				committed = currentCommitted;
				max = pool->peakMax;
			}
			omrthread_rwmutex_exit_write(mgmt->managementDataLock);
		}

		ctor = (*env)->FromReflectedMethod(env, memUsageConstructor);
		if (NULL == ctor) {
			return NULL;
		}

		return (*env)->NewObject(env, memoryUsage, ctor, init, used, committed, max);
	} else {
		/* NonHeap MemoryPool */
		J9MemorySegmentList *segList = getMemorySegmentList(javaVM, id);
		if (NULL != segList) {
			return processSegmentList(env, memoryUsage, memUsageConstructor, segList, mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].initialSize, mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].maxSize, &mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].peakSize, &mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].peakUsed, 1);
		} else {
			return NULL;
		}
	}
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getUsageImpl(JNIEnv *env, jobject beanInstance, jint id, jclass memoryUsage, jobject memUsageConstructor)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		/* Heap MemoryPool */
		UDATA totals[J9_GC_MANAGEMENT_MAX_POOL];
		UDATA frees[J9_GC_MANAGEMENT_MAX_POOL];
		jlong used = 0;
		jlong committed = 0;
		jlong peak = 0;
		jlong init = 0;
		jlong max = 0;
		jmethodID ctor = NULL;
		UDATA idx = getIndexFromPoolID(mgmt, id);
		J9MemoryPoolData *pool = &mgmt->memoryPools[idx];

		javaVM->memoryManagerFunctions->j9gc_pools_memory(javaVM, (id&J9VM_MANAGEMENT_POOL_HEAP_ID_MASK), &totals[0], &frees[0], FALSE);
		committed = totals[idx];
		used = committed - frees[idx];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		peak = (jlong) pool->peakUsed;
		init = (jlong) pool->initialSize;
		max = (jlong) pool->postCollectionMaxSize;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
		
		if (used > peak) {
			/* the current usage is above the recorded peak but no one else has noticed yet */
			omrthread_rwmutex_enter_write(mgmt->managementDataLock);
			if ((U_64)used > pool->peakUsed) {
				pool->peakUsed = used;
				pool->peakSize = committed;
				pool->peakMax = pool->postCollectionMaxSize;
			}
			omrthread_rwmutex_exit_write(mgmt->managementDataLock);
		}

		ctor = (*env)->FromReflectedMethod(env, memUsageConstructor);
		if (NULL == ctor) {
			return NULL;
		}

		return (*env)->NewObject( env, memoryUsage, ctor, init, used, committed, max );
	} else {
		/* NonHeap MemoryPool */
		J9MemorySegmentList *segList = getMemorySegmentList(javaVM, id);
		if (NULL != segList) {
			return processSegmentList(env, memoryUsage, memUsageConstructor, segList, mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].initialSize, mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].maxSize, &mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].peakSize, &mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].peakUsed, 0);
		} else {
			return NULL;
		}
	}
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getUsageThresholdImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jlong count = -1;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		count = (jlong) pool->usageThreshold;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
	}
	return count;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getUsageThresholdCountImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jlong count = -1;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		count = (jlong) pool->usageThresholdCrossedCount;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
	}
	return count;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_isCollectionUsageThresholdExceededImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jboolean result = JNI_FALSE;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		if ((0 != pool->collectionUsageThreshold) && (pool->collectionUsageThreshold < pool->postCollectionUsed)) {
			/* collectionUsageThreshold is enabled and smaller than postCollectionUsed */
			result = JNI_TRUE;
		}
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
	}
	return result;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_isCollectionUsageThresholdSupportedImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jboolean result = JNI_FALSE;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];
		UDATA max = 0;

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		max = (UDATA) pool->postCollectionMaxSize;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);

		if ((0 != max) && (0 != javaVM->memoryManagerFunctions->j9gc_is_collectionusagethreshold_supported(javaVM, (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK)))) {
			result = JNI_TRUE;
		}
	}
	return result;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_isUsageThresholdExceededImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jboolean result = JNI_FALSE;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		UDATA used = 0;
		UDATA totals[J9_GC_MANAGEMENT_MAX_POOL];
		UDATA frees[J9_GC_MANAGEMENT_MAX_POOL];
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		UDATA idx = getIndexFromPoolID(mgmt, id);
		J9MemoryPoolData *pool = &mgmt->memoryPools[idx];

		javaVM->memoryManagerFunctions->j9gc_pools_memory(javaVM, (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK), &totals[0], &frees[0], FALSE);
		used = totals[idx] - frees[idx];

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		if ((0 != pool->usageThreshold) && (pool->usageThreshold < used)) {
			/* usageThreshold is enabled and smaller than used */
			result = JNI_TRUE;
		}
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
	}
	return result;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_isUsageThresholdSupportedImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	jboolean result = JNI_FALSE;
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];
		UDATA max = 0;

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		max = (UDATA) pool->postCollectionMaxSize;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);

		if ((0 != max) && (0 != javaVM->memoryManagerFunctions->j9gc_is_usagethreshold_supported(javaVM, (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK)))) {
			result = JNI_TRUE;
		}
	}
	return result;
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_resetPeakUsageImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		/* Heap MemoryPool */
		UDATA used = 0;
		UDATA committed = 0;
		UDATA totals[J9_GC_MANAGEMENT_MAX_POOL];
		UDATA frees[J9_GC_MANAGEMENT_MAX_POOL];
		UDATA idx = getIndexFromPoolID(mgmt, id);
		J9MemoryPoolData *pool = &mgmt->memoryPools[idx];

		javaVM->memoryManagerFunctions->j9gc_pools_memory(javaVM, (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK), &totals[0], &frees[0], FALSE);
		committed = totals[idx];
		used = committed - frees[idx];

		omrthread_rwmutex_enter_write(mgmt->managementDataLock);
		pool->peakUsed = used;
		pool->peakSize = committed;
		pool->peakMax = pool->postCollectionMaxSize;
		omrthread_rwmutex_exit_write(mgmt->managementDataLock);
	} else {
		/* NonHeap MemoryPool */
		J9MemorySegmentList *segList = getMemorySegmentList(javaVM, id);
		if (NULL != segList) {
			processSegmentList(env, NULL, NULL, segList, 0, 0, &mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].peakSize, &mgmt->nonHeapMemoryPools[id-J9VM_MANAGEMENT_POOL_NONHEAP_SEG].peakUsed, 2);
		}
	}
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_setCollectionUsageThresholdImpl(JNIEnv *env, jobject beanInstance, jint id, jlong newThreshold)
{
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

		if (0 != javaVM->memoryManagerFunctions->j9gc_is_collectionusagethreshold_supported(javaVM, (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK))) {
			J9JavaLangManagementData *mgmt = javaVM->managementData;
			J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

			omrthread_rwmutex_enter_write(mgmt->managementDataLock);
			pool->collectionUsageThreshold = (U_64)newThreshold;
			pool->collectionUsageThresholdCrossedCount = 0;
			pool->notificationState &= ~COLLECTION_THRESHOLD_EXCEEDED;
			omrthread_rwmutex_exit_write(mgmt->managementDataLock);
		}
	}
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_setUsageThresholdImpl(JNIEnv *env, jobject beanInstance, jint id, jlong newThreshold)
{
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

		if (0 != javaVM->memoryManagerFunctions->j9gc_is_usagethreshold_supported(javaVM, (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK))) {
			J9JavaLangManagementData *mgmt = javaVM->managementData;
			J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];

			omrthread_rwmutex_enter_write(mgmt->managementDataLock);
			pool->usageThreshold = newThreshold;
			pool->usageThresholdCrossedCount = 0;
			pool->notificationState &= ~THRESHOLD_EXCEEDED;
			omrthread_rwmutex_exit_write(mgmt->managementDataLock);
		}
	}
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_MemoryPoolMXBeanImpl_getPreCollectionUsageImpl(JNIEnv *env, jobject beanInstance, jint id, jclass memoryUsage, jobject memUsageConstructor)
{
	if (0 != (id & J9VM_MANAGEMENT_POOL_HEAP)) {
		J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
		jlong used = 0;
		jlong committed = 0;
		jlong init = 0;
		jlong max = 0;
		J9JavaLangManagementData *mgmt = javaVM->managementData;
		J9MemoryPoolData *pool = &mgmt->memoryPools[getIndexFromPoolID(mgmt, id)];
		jmethodID ctor = NULL;

		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		used = (jlong) pool->preCollectionUsed;
		committed = (jlong) pool->preCollectionSize;
		init = (jlong) pool->initialSize;
		max = (jlong) pool->preCollectionMaxSize;
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);

		ctor = (*env)->FromReflectedMethod(env, memUsageConstructor);
		if (NULL == ctor) {
			return NULL;
		}

		memoryUsage = (*env)->NewObject(env, memoryUsage, ctor, init, used, committed, max);

		return memoryUsage;
	} else {
		return NULL;
	}
}

/* Helper to calculate memory usage of a segment list and do something with it.
	action:
		0 - check peak and update if necessary, return a MemoryUsage object for the current usage
		1 - check peak and update if necessary, return a MemoryUsage object for the peak usage
		2 - reset the peak to the current usage, return nothing */
static jobject
processSegmentList(JNIEnv *env, jclass memoryUsage, jobject memUsageConstructor, J9MemorySegmentList *segList, U_64 initialSize, I_64 maxSize, U_64 *storedPeakSize, U_64 *storedPeakUsed, UDATA action) {
	jlong used = 0;
	jlong committed = 0;
	jlong peakUsed = 0;
	jlong peakSize = 0;
	jobject memoryUsageObj = NULL;
	jmethodID ctor = NULL;
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	committed = used = 0;

	omrthread_monitor_enter(segList->segmentMutex);

	MEMORY_SEGMENT_LIST_DO(segList, seg)
		used += seg->heapAlloc - seg->heapBase;
		committed += seg->size;
	END_MEMORY_SEGMENT_LIST_DO(seg)

	omrthread_monitor_exit(segList->segmentMutex);

	omrthread_rwmutex_enter_write( mgmt->managementDataLock );

	peakUsed = (jlong)*storedPeakUsed;
	peakSize = (jlong)*storedPeakSize;

	if ((used > peakUsed) || (action == 2)) {
		/* the current usage is above the recorded peak but no one else has noticed yet */
		*storedPeakUsed = used;
		*storedPeakSize = committed;
		peakUsed = used;
		peakSize = committed;
	}

	omrthread_rwmutex_exit_write(mgmt->managementDataLock);

	if(2 != action) {
		ctor = (*env)->FromReflectedMethod(env, memUsageConstructor);
		if (NULL == ctor) {
			return NULL;
		}

		if (0 == action) {
			memoryUsageObj = (*env)->NewObject(env, memoryUsage, ctor, (jlong) initialSize, used, committed, (jlong) maxSize);
		} else if (1 == action) {
			memoryUsageObj = (*env)->NewObject(env, memoryUsage, ctor, (jlong) initialSize, peakUsed, peakSize, (jlong) maxSize);
		}
	}

	return memoryUsageObj;
}

static UDATA
getIndexFromPoolID(J9JavaLangManagementData *mgmt, UDATA id)
{
	UDATA idx = 0;
	for(; idx < mgmt->supportedMemoryPools; idx++) {
		if ((mgmt->memoryPools[idx].id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK) == (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK)) {
			break;
		}
	}
	return idx;
}

static J9MemorySegmentList *
getMemorySegmentList(J9JavaVM *javaVM, jint id)
{
	J9MemorySegmentList *segList = NULL;
	switch (id) {
	case J9VM_MANAGEMENT_POOL_NONHEAP_SEG_CLASSES:
		segList = javaVM->classMemorySegments;
		break;
	case J9VM_MANAGEMENT_POOL_NONHEAP_SEG_MISC:
		segList = javaVM->memorySegments;
		break;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	case J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_CODE:
		if (javaVM->jitConfig) {
			segList = javaVM->jitConfig->codeCacheList;
		}
		break;
	case J9VM_MANAGEMENT_POOL_NONHEAP_SEG_JIT_DATA:
		if (javaVM->jitConfig) {
			segList = javaVM->jitConfig->dataCacheList;
		}
		break;
#endif
	default:
		break;
	}
	return segList;
}
