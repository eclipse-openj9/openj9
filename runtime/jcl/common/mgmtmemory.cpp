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

#include "jni.h"
#include "j9.h"
#include "mgmtinit.h"
#include "jvminit.h"
#include "verbose_api.h"
#include "VMHelpers.hpp"

extern "C" {

static UDATA getIndexFromMemoryPoolID(J9JavaLangManagementData *mgmt, UDATA id);
static UDATA getIndexFromGCID(J9JavaLangManagementData *mgmt, UDATA id);

jobject JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getHeapMemoryUsageImpl(JNIEnv *env, jobject beanInstance, jclass memoryUsage, jobject memUsageConstructor)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	jlong committed = javaVM->memoryManagerFunctions->j9gc_heap_total_memory(javaVM);
	jlong used = committed - javaVM->memoryManagerFunctions->j9gc_heap_free_memory(javaVM);
	jmethodID ctor = env->FromReflectedMethod(memUsageConstructor);

	if (NULL == ctor) {
		return NULL;
	}

	return env->NewObject(memoryUsage, ctor, (jlong)javaVM->managementData->initialHeapSize, used, committed, (jlong)javaVM->managementData->maximumHeapSize);
}

jobject JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getNonHeapMemoryUsageImpl(JNIEnv *env, jobject beanInstance, jclass memoryUsage, jobject memUsageConstructor)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	I_64 used = 0;
	jlong committed = 0;
	jlong initial = 0;
	jmethodID ctor = NULL;
	J9MemorySegmentList *segList = javaVM->classMemorySegments;
	J9ClassLoaderWalkState walkState;
	UDATA idx = 0;

	omrthread_monitor_enter(segList->segmentMutex);

	MEMORY_SEGMENT_LIST_DO(segList, seg)
		used += seg->heapAlloc - seg->heapBase;
		committed += seg->size;
	END_MEMORY_SEGMENT_LIST_DO(seg)

	omrthread_monitor_exit(segList->segmentMutex);

	/* The classTableMutex is held while allocating RAM class fragments from free lists. @see createramclass.c#internalCreateRAMClassFromROMClass() */
	omrthread_monitor_enter(javaVM->classTableMutex);
	J9ClassLoader *classLoader = javaVM->internalVMFunctions->allClassLoadersStartDo(&walkState, javaVM, 0);
	while (NULL != classLoader) {
		J9RAMClassFreeLists *sub4gBlockPtr = &classLoader->sub4gBlock;
		J9RAMClassFreeLists *frequentlyAccessedBlockPtr = &classLoader->frequentlyAccessedBlock;
		J9RAMClassFreeLists *inFrequentlyAccessedBlockPtr = &classLoader->inFrequentlyAccessedBlock;
		UDATA *ramClassSub4gUDATABlockFreeListPtr = sub4gBlockPtr->ramClassUDATABlockFreeList;
		UDATA *ramClassFreqUDATABlockFreeListPtr = frequentlyAccessedBlockPtr->ramClassUDATABlockFreeList;
		UDATA *ramClassInFreqUDATABlockFreeListPtr = inFrequentlyAccessedBlockPtr->ramClassUDATABlockFreeList;

		VM_VMHelpers::subtractUDATABlockChain(ramClassSub4gUDATABlockFreeListPtr, &used);
		VM_VMHelpers::subtractUDATABlockChain(ramClassFreqUDATABlockFreeListPtr, &used);
		VM_VMHelpers::subtractUDATABlockChain(ramClassInFreqUDATABlockFreeListPtr, &used);

		VM_VMHelpers::subtractFreeListBlocks(sub4gBlockPtr, &used);
		VM_VMHelpers::subtractFreeListBlocks(frequentlyAccessedBlockPtr, &used);
		VM_VMHelpers::subtractFreeListBlocks(inFrequentlyAccessedBlockPtr, &used);

		classLoader = javaVM->internalVMFunctions->allClassLoadersNextDo(&walkState);
	}
	javaVM->internalVMFunctions->allClassLoadersEndDo(&walkState);
	omrthread_monitor_exit(javaVM->classTableMutex);

	segList = javaVM->memorySegments;
	omrthread_monitor_enter(segList->segmentMutex);

	MEMORY_SEGMENT_LIST_DO(segList, seg)
		used += seg->heapAlloc - seg->heapBase;
		committed += seg->size;
	END_MEMORY_SEGMENT_LIST_DO(seg)

	omrthread_monitor_exit(segList->segmentMutex);

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	if (NULL != javaVM->jitConfig) {
		segList = javaVM->jitConfig->codeCacheList;
		omrthread_monitor_enter(segList->segmentMutex);

		MEMORY_SEGMENT_LIST_DO(segList, seg)
		{
			/* Set default values for warmAlloc and coldAlloc pointers */
			UDATA warmAlloc = (UDATA)seg->heapBase;
			UDATA coldAlloc = (UDATA)seg->heapTop;

			/* The JIT code cache grows from both ends of the segment: the warmAlloc pointer upwards from the start of the segment
			 * and the coldAlloc pointer downwards from the end of the segment. The free space in a JIT code cache segment is the
			 * space between the warmAlloc and coldAlloc pointers. See compiler/runtime/OMRCodeCache.hpp, the contract with the JVM is
			 * that the address of the TR::CodeCache structure is stored at the beginning of the segment.
			 */
			UDATA *mccCodeCache = *((UDATA **)seg->heapBase);
			if (NULL != mccCodeCache) {
				warmAlloc = (UDATA)javaVM->jitConfig->codeCacheWarmAlloc(mccCodeCache);
				coldAlloc = (UDATA)javaVM->jitConfig->codeCacheColdAlloc(mccCodeCache);
			}
			used += seg->size - (coldAlloc - warmAlloc);
			committed += seg->size;
		}
		END_MEMORY_SEGMENT_LIST_DO(seg)

		omrthread_monitor_exit(segList->segmentMutex);

		segList = javaVM->jitConfig->dataCacheList;
		omrthread_monitor_enter(segList->segmentMutex);

		MEMORY_SEGMENT_LIST_DO(segList, seg)
			used += seg->heapAlloc - seg->heapBase;
			committed += seg->size;
		END_MEMORY_SEGMENT_LIST_DO(seg)

		omrthread_monitor_exit(segList->segmentMutex);
	}
#endif /* defined(J9VM_INTERP_NATIVE_SUPPORT) */

	for (idx = 0; idx < mgmt->supportedNonHeapMemoryPools; ++idx) {
		initial += mgmt->nonHeapMemoryPools[idx].initialSize;
	}
	ctor = env->FromReflectedMethod(memUsageConstructor);
	if (NULL == ctor) {
		return NULL;
	}

	return env->NewObject(memoryUsage, ctor, initial, used, committed, (jlong)-1);
}

jint JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getObjectPendingFinalizationCountImpl(JNIEnv *env, jobject beanInstance)
{
#if defined(J9VM_GC_FINALIZATION)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	return (jint)javaVM->memoryManagerFunctions->j9gc_get_objects_pending_finalization_count(javaVM);
#else /* defined(J9VM_GC_FINALIZATION) */
	return (jint)0;
#endif /* defined(J9VM_GC_FINALIZATION) */
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_isVerboseImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	return J9_ARE_ANY_BITS_SET(javaVM->verboseLevel, VERBOSE_GC);
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setVerboseImpl(JNIEnv *env, jobject beanInstance, jboolean flag)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9VerboseSettings verboseOptions;

	memset(&verboseOptions, 0, sizeof(verboseOptions));
	if (NULL != javaVM->setVerboseState) {
		verboseOptions.gc = flag ? VERBOSE_SETTINGS_SET : VERBOSE_SETTINGS_CLEAR;
		javaVM->setVerboseState(javaVM, &verboseOptions, NULL);
	}
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_createMemoryManagers(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	jclass memBean = NULL;
	jstring childName = NULL;
	jmethodID helperID = NULL;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	jint id = 0;
	UDATA idx = 0;

	/*
	 * This method performs the following Java code:
	 * createMemoryManagerHelper( "J9 non-heap memory manager", 0, false );
	 * for supportedGarbageCollectors
	 * createMemoryManagerHelper( gcName, gcID, true);
	 */

	memBean = env->GetObjectClass(beanInstance);
	if (NULL == memBean) {
		return;
	}

	helperID = env->GetMethodID(memBean, "createMemoryManagerHelper", "(Ljava/lang/String;IZ)V");
	if (NULL == helperID) {
		return;
	}

	childName = env->NewStringUTF("J9 non-heap manager");
	if (NULL == childName) {
		return;
	}

	env->CallVoidMethod(beanInstance, helperID, childName, J9VM_MANAGEMENT_POOL_NONHEAP, JNI_FALSE);
	if (env->ExceptionCheck()) {
		return;
	}

	for (idx = 0; idx < mgmt->supportedCollectors; ++idx) {
		id = (jint)mgmt->garbageCollectors[idx].id;
		childName = env->NewStringUTF(mgmt->garbageCollectors[idx].name);
		if (NULL == childName) {
			return;
		}

		env->CallVoidMethod(beanInstance, helperID, childName, id, JNI_TRUE);
	}
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_createMemoryPools(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	jclass memBean = NULL;
	jstring childName = NULL;
	jmethodID helperID = NULL;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	jint id = 0;
	UDATA idx = 0;

	memBean = env->GetObjectClass(beanInstance);
	if (NULL == memBean) {
		return;
	}

	helperID = env->GetMethodID(memBean, "createMemoryPoolHelper", "(Ljava/lang/String;IZ)V");
	if (NULL == helperID) {
		return;
	}

	/* Heap Memory Pools */
	for (idx = 0; idx < mgmt->supportedMemoryPools; ++idx) {
		id = (jint)mgmt->memoryPools[idx].id;
		childName = env->NewStringUTF(mgmt->memoryPools[idx].name);
		if (NULL == childName) {
			return;
		}

		env->CallVoidMethod(beanInstance, helperID, childName, id, JNI_TRUE);
		if (env->ExceptionCheck()) {
			return;
		}
	}

	/* NonHeap Memory Pools */
	for (idx = 0; idx < mgmt->supportedNonHeapMemoryPools; ++idx) {
		id = (jint)mgmt->nonHeapMemoryPools[idx].id;
		childName = env->NewStringUTF(mgmt->nonHeapMemoryPools[idx].name);
		if (NULL == childName) {
			return;
		}

		env->CallVoidMethod(beanInstance, helperID, childName, id, JNI_FALSE);
		if (env->ExceptionCheck()) {
			return;
		}
	}
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getMaxHeapSizeLimitImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	return javaVM->memoryManagerFunctions->j9gc_get_maximum_heap_size(javaVM);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getMaxHeapSizeImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	UDATA softmx = javaVM->memoryManagerFunctions->j9gc_get_softmx(javaVM);

	/* if no softmx has been set, report -Xmx instead as it is the current max heap size */
	if (0 == softmx) {
		softmx = javaVM->memoryManagerFunctions->j9gc_get_maximum_heap_size(javaVM);
	}
	return softmx;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getMinHeapSizeImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	return javaVM->memoryManagerFunctions->j9gc_get_initial_heap_size(javaVM);
}

void JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setMaxHeapSizeImpl(JNIEnv *env, jobject beanInstance, jlong newsoftmx)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	javaVM->memoryManagerFunctions->j9gc_set_softmx(javaVM, (UDATA)newsoftmx);
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setSharedClassCacheSoftmxBytesImpl(JNIEnv *env, jobject beanInstance, jlong value)
{
	jboolean ret = JNI_FALSE;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		if (0 != javaVM->sharedClassConfig->setMinMaxBytes(javaVM, (U_32)value, -1, -1, -1, -1)) {
			ret = JNI_TRUE;
		}
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return ret;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setSharedClassCacheMinAotBytesImpl(JNIEnv *env, jobject beanInstance, jlong value)
{
	jboolean ret = JNI_FALSE;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		if (0 != javaVM->sharedClassConfig->setMinMaxBytes(javaVM, (U_32)-1, (I_32)value, -1, -1, -1)) {
			ret = JNI_TRUE;
		}
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return ret;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setSharedClassCacheMaxAotBytesImpl(JNIEnv *env, jobject beanInstance, jlong value)
{
	jboolean ret = JNI_FALSE;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		if (0 != javaVM->sharedClassConfig->setMinMaxBytes(javaVM, (U_32)-1, -1, (I_32)value, -1, -1)) {
			ret = JNI_TRUE;
		}
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return ret;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setSharedClassCacheMinJitDataBytesImpl(JNIEnv *env, jobject beanInstance, jlong value)
{
	jboolean ret = JNI_FALSE;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		if (0 != javaVM->sharedClassConfig->setMinMaxBytes(javaVM, (U_32)-1, -1, -1, (I_32)value, -1)) {
			ret = JNI_TRUE;
		}
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return ret;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_setSharedClassCacheMaxJitDataBytesImpl(JNIEnv *env, jobject beanInstance, jlong value)
{
	jboolean ret = JNI_FALSE;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		if (0 != javaVM->sharedClassConfig->setMinMaxBytes(javaVM, (U_32)-1, -1, -1, -1, (I_32)value)) {
			ret = JNI_TRUE;
		}
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return ret;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getSharedClassCacheSoftmxUnstoredBytesImpl(JNIEnv *env, jobject beanInstance)
{
	U_32 ret = 0;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getUnstoredBytes(javaVM, &ret, NULL, NULL);
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getSharedClassCacheMaxAotUnstoredBytesImpl(JNIEnv *env, jobject beanInstance)
{
	U_32 ret = 0;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getUnstoredBytes(javaVM, NULL, &ret, NULL);
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getSharedClassCacheMaxJitDataUnstoredBytesImpl(JNIEnv *env, jobject beanInstance)
{
	U_32 ret = 0;

#if defined(J9VM_OPT_SHARED_CLASSES)
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;

	if (NULL != javaVM->sharedClassConfig) {
		javaVM->sharedClassConfig->getUnstoredBytes(javaVM, NULL, NULL, &ret);
	}
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
	return (jlong)ret;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_isSetMaxHeapSizeSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	return JNI_TRUE;
}

jstring JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getGCModeImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	const char *gcMode = javaVM->memoryManagerFunctions->j9gc_get_gcmodestring(javaVM);

	if (NULL != gcMode) {
		return env->NewStringUTF(gcMode);
	} else {
		return NULL;
	}
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getGCMainThreadCpuUsedImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read(mgmt->managementDataLock);
	jlong result = (jlong)mgmt->gcMainCpuTime;
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);

	return result;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getGCWorkerThreadsCpuUsedImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read(mgmt->managementDataLock);
	jlong result = (jlong)mgmt->gcWorkerCpuTime;
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);

	return result;
}

jint JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getMaximumGCThreadsImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read(mgmt->managementDataLock);
	jint result = (jint)mgmt->gcMaxThreads;
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);

	return result;
}

jint JNICALL
Java_com_ibm_java_lang_management_internal_MemoryMXBeanImpl_getCurrentGCThreadsImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read(mgmt->managementDataLock);
	jint result = (jint)mgmt->gcCurrentThreads;
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);

	return result;
}

/* Implementation of the main loop of a thread that processes and dispatches memory usage notifications to Java handlers. */
void JNICALL
Java_com_ibm_lang_management_internal_MemoryNotificationThread_processNotificationLoop(JNIEnv *env, jobject threadInstance)
{
	/* currently, the only notification queue is for the heap */
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	jclass threadClass = NULL;
	jclass stringClass = NULL;
	jmethodID helperMemID = NULL;
	jmethodID helperGCID = NULL;
	J9MemoryPoolData *pool = NULL;
	jstring poolName = NULL;
	jstring gcName = NULL;
	U_32 idx = 0;
	jstring poolNames[J9VM_MAX_HEAP_MEMORYPOOL_COUNT];
	jstring gcNames[J9_GC_MANAGEMENT_MAX_COLLECTOR];
	J9MemoryNotification *notification = NULL;

	PORT_ACCESS_FROM_ENV(env);

	/* cache poolNames and gcNames */
	for (idx = 0; idx < mgmt->supportedMemoryPools; ++idx) {
		poolNames[idx] = env->NewStringUTF(mgmt->memoryPools[idx].name);
		if (NULL == poolNames[idx]) {
			return;
		}
	}

	for (idx = 0; idx < mgmt->supportedCollectors; ++idx) {
		gcNames[idx] = env->NewStringUTF(mgmt->garbageCollectors[idx].name);
		if (NULL == gcNames[idx]) {
			return;
		}
	}

	/* locate the helper that constructs and dispatches the Java notification objects */
	threadClass = env->FindClass("com/ibm/lang/management/internal/MemoryNotificationThread");
	if (NULL == threadClass) {
		return;
	}

	stringClass = env->FindClass("java/lang/String");
	if (NULL == stringClass) {
		return;
	}

	helperMemID = env->GetMethodID(threadClass, "dispatchMemoryNotificationHelper", "(Ljava/lang/String;JJJJJJZ)V");
	if (NULL == helperMemID) {
		return;
	}

	helperGCID = env->GetMethodID(threadClass, "dispatchGCNotificationHelper", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JJJ[J[J[J[J[J[J[JJ)V");
	if (NULL == helperGCID) {
		return;
	}

	omrthread_rwmutex_enter_write(mgmt->managementDataLock);
	mgmt->notificationEnabled = 1;
	omrthread_rwmutex_exit_write(mgmt->managementDataLock);

	while (1) {
		if (env->PushLocalFrame(16) < 0) {
			/* out of memory */
			return;
		}

		/* wait on the notification queue monitor until a notification is posted */
		j9thread_monitor_enter(mgmt->notificationMonitor);
		while (0 == mgmt->notificationsPending) {
			j9thread_monitor_wait(mgmt->notificationMonitor);
		}
		mgmt->notificationsPending -= 1;
		notification = (J9MemoryNotification *)mgmt->notificationQueue;
		mgmt->notificationQueue = notification->next;
		j9thread_monitor_exit(mgmt->notificationMonitor);

		/* check notification type */
		if (NOTIFIER_SHUTDOWN_REQUEST == notification->type) {
			/* shutdown request */
			j9mem_free_memory(notification);
			break;
		} else if (END_OF_GARBAGE_COLLECTION == notification->type) {
			/* dispatch GC Notification */
			jstring gcAction = NULL;
			jstring gcCause = NULL;
			J9GarbageCollectionInfo *gcInfo = notification->gcInfo;
			jlongArray initialArray = NULL;
			jlongArray preUsedArray = NULL;
			jlongArray preCommittedArray = NULL;
			jlongArray preMaxArray = NULL;
			jlongArray postUsedArray = NULL;
			jlongArray postCommittedArray = NULL;
			jlongArray postMaxArray = NULL;

			initialArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == initialArray) {
				return;
			}
			preUsedArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == preUsedArray) {
				return;
			}
			preCommittedArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == preCommittedArray) {
				return;
			}
			preMaxArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == preMaxArray) {
				return;
			}
			postUsedArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == postUsedArray) {
				return;
			}
			postCommittedArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == postCommittedArray) {
				return;
			}
			postMaxArray = env->NewLongArray(gcInfo->arraySize);
			if (NULL == postMaxArray) {
				return;
			}

			gcName = gcNames[getIndexFromGCID(mgmt, gcInfo->gcID)];
			if (NULL == gcName) {
				return;
			}
			gcAction = env->NewStringUTF(gcInfo->gcAction);
			if (NULL == gcAction) {
				return;
			}
			gcCause = env->NewStringUTF(gcInfo->gcCause);
			if (NULL == gcCause) {
				return;
			}

			env->SetLongArrayRegion(initialArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->initialSize[0]);
			if (env->ExceptionCheck()) {
				return;
			}
			env->SetLongArrayRegion(preUsedArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->preUsed[0]);
			if (env->ExceptionCheck()) {
				return;
			}
			env->SetLongArrayRegion(preCommittedArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->preCommitted[0]);
			if (env->ExceptionCheck()) {
				return;
			}
			env->SetLongArrayRegion(preMaxArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->preMax[0]);
			if (env->ExceptionCheck()) {
				return;
			}
			env->SetLongArrayRegion(postUsedArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->postUsed[0]);
			if (env->ExceptionCheck()) {
				return;
			}
			env->SetLongArrayRegion(postCommittedArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->postCommitted[0]);
			if (env->ExceptionCheck()) {
				return;
			}
			env->SetLongArrayRegion(postMaxArray, 0, gcInfo->arraySize, (jlong *)&gcInfo->postMax[0]);
			if (env->ExceptionCheck()) {
				return;
			}

			env->CallVoidMethod(
					threadInstance,
					helperGCID,
					gcName,
					gcAction,
					gcCause,
					(jlong)gcInfo->index,
					(jlong)gcInfo->startTime,
					(jlong)gcInfo->endTime,
					initialArray,
					preUsedArray,
					preCommittedArray,
					preMaxArray,
					postUsedArray,
					postCommittedArray,
					postMaxArray,
					(jlong)notification->sequenceNumber);
			if (env->ExceptionCheck()) {
				return;
			}
		} else {
			/* dispatch  usage threshold Notification */
			memoryPoolUsageThreshold *usageThreshold = notification->usageThreshold;
			idx = (U_32)getIndexFromMemoryPoolID(mgmt, usageThreshold->poolID);
			pool = &mgmt->memoryPools[idx];
			poolName = poolNames[idx];
			if (THRESHOLD_EXCEEDED == notification->type) {
				/* heap usage threshold exceeded */
				env->CallVoidMethod(
						threadInstance,
						helperMemID,
						poolName,
						(jlong)pool->initialSize,
						(jlong)usageThreshold->usedSize,
						(jlong)usageThreshold->totalSize,
						(jlong)usageThreshold->maxSize,
						(jlong)usageThreshold->thresholdCrossingCount,
						(jlong)notification->sequenceNumber,
						JNI_FALSE);
				if (env->ExceptionCheck()) {
					return;
				}
			} else { /* COLLECTION_THRESHOLD_EXCEEDED == notification->type) */
				/* heap collection usage threshold exceeded */
				env->CallVoidMethod(
						threadInstance,
						helperMemID,
						poolName,
						pool->initialSize,
						(jlong)usageThreshold->usedSize,
						(jlong)usageThreshold->totalSize,
						(jlong)usageThreshold->maxSize,
						(jlong)usageThreshold->thresholdCrossingCount,
						(jlong)notification->sequenceNumber,
						JNI_TRUE);
				if (env->ExceptionCheck()) {
					return;
				}
			}
		}

		/* clean up the notification */
		if (NULL != notification->usageThreshold) {
			j9mem_free_memory(notification->usageThreshold);
		} else if (NULL != notification->gcInfo) {
			j9mem_free_memory(notification->gcInfo);
		}
		j9mem_free_memory(notification);

		if (env->ExceptionCheck()) {
			/* if the dispatcher throws, just kill the thread for now */
			break;
		}

		env->PopLocalFrame(NULL);
	}
}

/* Sends a shutdown request to a notification queue. */
void JNICALL
Java_com_ibm_lang_management_internal_MemoryNotificationThreadShutdown_sendShutdownNotification(JNIEnv *env, jobject instance)
{
	/* currently, the only queue is the heap usage notification queue */
	J9JavaVM *javaVM = ((J9VMThread *)env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	J9MemoryNotification *notification = NULL;
	J9MemoryNotification *next = NULL;
	PORT_ACCESS_FROM_ENV(env);

	/* allocate a notification */
	notification = (J9MemoryNotification *)j9mem_allocate_memory(sizeof(*notification), J9MEM_CATEGORY_VM_JCL);
	if (NULL != notification) {
		/* set up a shutdown notification */
		notification->type = NOTIFIER_SHUTDOWN_REQUEST;
		notification->usageThreshold = NULL;
		notification->gcInfo = NULL;
		notification->next = NULL;

		omrthread_rwmutex_enter_write(mgmt->managementDataLock);
		mgmt->notificationEnabled = 0;
		omrthread_rwmutex_exit_write(mgmt->managementDataLock);

		/* replace the queue with just this notification - the notifier thread does not care that the pending count might be >1, it will not process anything after the shutdown request */
		j9thread_monitor_enter(mgmt->notificationMonitor);
		next = (J9MemoryNotification *)mgmt->notificationQueue;
		mgmt->notificationQueue = notification;

		/* free the old queue entries if any */
		while (NULL != next) {
			J9MemoryNotification *temp = next;
			next = next->next;
			if (NULL != temp->usageThreshold) {
				j9mem_free_memory(temp->usageThreshold);
			} else if (NULL != temp->gcInfo) {
				j9mem_free_memory(temp->gcInfo);
			}
			j9mem_free_memory(temp);
		}

		/* notify the notification thread */
		mgmt->notificationsPending = 1;
		j9thread_monitor_notify(mgmt->notificationMonitor);
		j9thread_monitor_exit(mgmt->notificationMonitor);
	}
}

static UDATA
getIndexFromMemoryPoolID(J9JavaLangManagementData *mgmt, UDATA id)
{
	UDATA idx = 0;
	for (; idx < mgmt->supportedMemoryPools; ++idx) {
		if ((mgmt->memoryPools[idx].id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK) == (id & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK)) {
			break;
		}
	}
	return idx;
}

static UDATA
getIndexFromGCID(J9JavaLangManagementData *mgmt, UDATA id)
{
	UDATA idx = 0;

	for (; idx < mgmt->supportedCollectors; ++idx) {
		if ((J9VM_MANAGEMENT_GC_HEAP_ID_MASK & mgmt->garbageCollectors[idx].id) == (J9VM_MANAGEMENT_GC_HEAP_ID_MASK & id)) {
			break;
		}
	}
	return idx;
}

} /* extern "C" */
