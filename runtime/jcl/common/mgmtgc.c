/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "jni.h"
#include "j9.h"
#include "jclglob.h"
#include "jclprots.h"
#include "mgmtinit.h"
#include "jniidcacheinit.h"

typedef enum {
	FIELD_TOTAL_GC_TIME,
	FIELD_LASTGC_START_TIME,
	FIELD_LASTGC_END_TIME,
	FIELD_COLLECTION_COUNT,
	FIELD_MEMORY_USED,
	FIELD_TOTAL_MEMORY_FREED,
	FIELD_TOTAL_COMPACTS
} GarbageCollectorField;

#define GC_FIELD_TOTAL_GC_TIME

static UDATA getIndexFromCollectorID(J9JavaLangManagementData *mgmt, UDATA id);
static jlong getCollectorField(JNIEnv *env, jint id, GarbageCollectorField field);

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getCollectionCountImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_COLLECTION_COUNT);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getCollectionTimeImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_TOTAL_GC_TIME);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getLastCollectionStartTimeImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_LASTGC_START_TIME);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getLastCollectionEndTimeImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_LASTGC_END_TIME);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getTotalMemoryFreedImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_TOTAL_MEMORY_FREED);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getTotalCompactsImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_TOTAL_COMPACTS);
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_GarbageCollectorMXBeanImpl_getMemoryUsedImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	return getCollectorField(env, id, FIELD_MEMORY_USED);
}

static UDATA
getIndexFromCollectorID(J9JavaLangManagementData *mgmt, UDATA id)
{
	UDATA idx = 0;

	for(; idx < mgmt->supportedCollectors; ++idx) {
		if ((J9VM_MANAGEMENT_GC_HEAP_ID_MASK & mgmt->garbageCollectors[idx].id) == (J9VM_MANAGEMENT_GC_HEAP_ID_MASK & id)) {
			break;
		}
	}
	return idx;
}

jobject JNICALL
Java_com_ibm_lang_management_internal_ExtendedGarbageCollectorMXBeanImpl_getLastGcInfoImpl(JNIEnv *env, jobject beanInstance, jint id)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	J9GarbageCollectorData *gc = &mgmt->garbageCollectors[getIndexFromCollectorID(mgmt, (UDATA) id)];
	J9GarbageCollectionInfo *gcInfo = &gc->lastGcInfo;
	jclass gcBean = NULL;
	jmethodID callBackID = NULL;

	jclass stringClass = NULL;
	U_32 idx = 0;
	jlongArray initialArray = NULL;
	jlongArray preUsedArray = NULL;
	jlongArray preCommittedArray = NULL;
	jlongArray preMaxArray = NULL;
	jlongArray postUsedArray = NULL;
	jlongArray postCommittedArray = NULL;
	jlongArray postMaxArray = NULL;
	jobjectArray poolNamesArray = NULL;

	/* collectionCount */
	if (0 == gc->lastGcInfo.index) {
		/* return NULL before the first collection */
		goto fail;
	}

	gcBean = (*env)->GetObjectClass(env, beanInstance);
	if (NULL == gcBean) {
		goto fail;
	}

	if (!initializeJavaLangStringIDCache(env)) {
		goto fail;
	}
	stringClass = JCL_CACHE_GET(env, CLS_java_lang_String);

	callBackID = JCL_CACHE_GET(env, MID_com_ibm_lang_management_internal_ExtendedGarbageCollectorMXBeanImpl_buildGcInfo);
	if (NULL == callBackID) {
		callBackID = (*env)->GetStaticMethodID(env, gcBean, "buildGcInfo", "(JJJ[Ljava/lang/String;[J[J[J[J[J[J[J)Lcom/sun/management/GcInfo;");
		if (NULL == callBackID) {
			goto fail;
		}
		JCL_CACHE_SET(env, MID_com_ibm_lang_management_internal_ExtendedGarbageCollectorMXBeanImpl_buildGcInfo, callBackID);
	}

	initialArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == initialArray) {
		goto fail;
	}
	preUsedArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == preUsedArray) {
		goto fail;
	}
	preCommittedArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == preCommittedArray) {
		goto fail;
	}
	preMaxArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == preMaxArray) {
		goto fail;
	}
	postUsedArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == postUsedArray) {
		goto fail;
	}
	postCommittedArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == postCommittedArray) {
		goto fail;
	}
	postMaxArray = (*env)->NewLongArray(env, gcInfo->arraySize);
	if (NULL == postMaxArray) {
		goto fail;
	}
	poolNamesArray = (jobjectArray)(*env)->NewObjectArray(env, gcInfo->arraySize, stringClass, NULL);
	if (NULL == poolNamesArray) {
		goto fail;
	}

	omrthread_rwmutex_enter_read(mgmt->managementDataLock);

	(*env)->SetLongArrayRegion(env, initialArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->initialSize[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}
	(*env)->SetLongArrayRegion(env, preUsedArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->preUsed[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}
	(*env)->SetLongArrayRegion(env, preCommittedArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->preCommitted[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}
	(*env)->SetLongArrayRegion(env, preMaxArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->preMax[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}
	(*env)->SetLongArrayRegion(env, postUsedArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->postUsed[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}
	(*env)->SetLongArrayRegion(env, postCommittedArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->postCommitted[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}
	(*env)->SetLongArrayRegion(env, postMaxArray, 0, gcInfo->arraySize, (jlong *) &gcInfo->postMax[0]);
	if ((*env)->ExceptionCheck(env)) {
		goto fail2;
	}

	for (idx = 0; idx < gcInfo->arraySize; ++idx) {
		jstring poolName = NULL;
		if (mgmt->supportedMemoryPools > idx) {
			poolName = (*env)->NewStringUTF(env, mgmt->memoryPools[idx].name);
		} else {
			poolName = (*env)->NewStringUTF(env, mgmt->nonHeapMemoryPools[idx - mgmt->supportedMemoryPools].name);
		}
		if (NULL == poolName) {
			goto fail2;
		}

		(*env)->SetObjectArrayElement(env, poolNamesArray, idx, poolName);
		if ((*env)->ExceptionCheck(env)) {
			goto fail2;
		}
		(*env)->DeleteLocalRef(env, poolName);
	}
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);

	return (*env)->CallStaticObjectMethod(env, gcBean, callBackID,
			(jlong)gcInfo->index, (jlong)gcInfo->startTime, (jlong)gcInfo->endTime,
			poolNamesArray,
			initialArray,
			preUsedArray,
			preCommittedArray,
			preMaxArray,
			postUsedArray,
			postCommittedArray,
			postMaxArray);
fail2:
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);
fail:
	return NULL;
}

static jlong
getCollectorField(JNIEnv *env, jint id, GarbageCollectorField field)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	jlong result = 0;
	J9GarbageCollectorData *gc = &mgmt->garbageCollectors[getIndexFromCollectorID(mgmt, (UDATA) id)];

	omrthread_rwmutex_enter_read(mgmt->managementDataLock);
	switch (field) {
	case FIELD_TOTAL_GC_TIME :
		result = (jlong) gc->totalGCTime;
		break;
	case FIELD_LASTGC_START_TIME :
		/* lastGcInfo.startTime is the start time of last GC since the Java virtual machine was started(ms),
		 * LastCollectionStartTime is the start time of last GC in timestamp(ms)
		 */
		result = (jlong) (gc->lastGcInfo.startTime + mgmt->vmStartTime);
		break;
	case FIELD_LASTGC_END_TIME :
		/* lastGcInfo.endTime is the end time of last GC since the Java virtual machine was started(ms),
		 * LastCollectionEndTime is the end time of last GC in timestamp(ms)
		 */
 		result = (jlong) (gc->lastGcInfo.endTime + mgmt->vmStartTime);
 		break;
	case FIELD_COLLECTION_COUNT :
		result = (jlong) gc->lastGcInfo.index;
		break;
	case FIELD_MEMORY_USED :
		result = (jlong) gc->memoryUsed;
		break;
	case FIELD_TOTAL_MEMORY_FREED :
		result = (jlong) gc->totalMemoryFreed;
		break;
	case FIELD_TOTAL_COMPACTS :
		result = (jlong) gc->totalCompacts;
		break;
	default:
		break;
	}
	omrthread_rwmutex_exit_read(mgmt->managementDataLock);

	return result;
}
