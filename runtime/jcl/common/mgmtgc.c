/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

	U_32 idx = 0;
	jlongArray initialArray = NULL;
	jlongArray preUsedArray = NULL;
	jlongArray preCommittedArray = NULL;
	jlongArray preMaxArray = NULL;
	jlongArray postUsedArray = NULL;
	jlongArray postCommittedArray = NULL;
	jlongArray postMaxArray = NULL;

	jlong* initialArrayElems = NULL;
	jlong* preUsedArrayElems = NULL;
	jlong* preCommittedArrayElems = NULL;
	jlong* preMaxArrayElems = NULL;
	jlong* postUsedArrayElems = NULL;
	jlong* postCommittedArrayElems = NULL;
	jlong* postMaxArrayElems = NULL;

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

	callBackID = JCL_CACHE_GET(env, MID_com_ibm_lang_management_internal_ExtendedGarbageCollectorMXBeanImpl_buildGcInfo);
	if (NULL == callBackID) {
		callBackID = (*env)->GetStaticMethodID(env, gcBean, "buildGcInfo", "(JJJ[J[J[J[J[J[J[J)Lcom/sun/management/GcInfo;");
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

	{
		jboolean isCopy = JNI_FALSE;
		initialArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, initialArray, &isCopy);
		if (NULL == initialArrayElems) {
			goto fail2;
		}
		preUsedArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, preUsedArray, &isCopy);
		if (NULL == preUsedArrayElems) {
			goto fail2;
		}
		preCommittedArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, preCommittedArray, &isCopy);
		if (NULL == preCommittedArrayElems) {
			goto fail2;
		}
		preMaxArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, preMaxArray, &isCopy);
		if (NULL == preMaxArrayElems) {
			goto fail2;
		}
		postUsedArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, postUsedArray, &isCopy);
		if (NULL == postUsedArrayElems) {
			goto fail2;
		}
		postCommittedArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, postCommittedArray, &isCopy);
		if (NULL == postCommittedArrayElems) {
			goto fail2;
		}
		postMaxArrayElems = (jlong*) (*env)->GetPrimitiveArrayCritical(env, postMaxArray, &isCopy);
		if (NULL == postMaxArrayElems) {
			goto fail2;
		}
		omrthread_rwmutex_enter_read(mgmt->managementDataLock);
		for (idx = 0; idx < gcInfo->arraySize; idx++) {
			initialArrayElems[idx] = gcInfo->initialSize[idx];
			preUsedArrayElems[idx] = gcInfo->preUsed[idx];
			preCommittedArrayElems[idx] = gcInfo->preCommitted[idx];
			preMaxArrayElems[idx] = gcInfo->preMax[idx];
			postUsedArrayElems[idx] = gcInfo->postUsed[idx];
			postCommittedArrayElems[idx] = gcInfo->postCommitted[idx];
			postMaxArrayElems[idx] = gcInfo->postMax[idx];
		}
		omrthread_rwmutex_exit_read(mgmt->managementDataLock);
		(*env)->ReleasePrimitiveArrayCritical(env, initialArray, initialArrayElems, 0);
		(*env)->ReleasePrimitiveArrayCritical(env, preUsedArray, preUsedArrayElems, 0);
		(*env)->ReleasePrimitiveArrayCritical(env, preCommittedArray, preCommittedArrayElems, 0);
		(*env)->ReleasePrimitiveArrayCritical(env, preMaxArray, preMaxArrayElems, 0);
		(*env)->ReleasePrimitiveArrayCritical(env, postUsedArray, postUsedArrayElems, 0);
		(*env)->ReleasePrimitiveArrayCritical(env, postCommittedArray, postCommittedArrayElems, 0);
		(*env)->ReleasePrimitiveArrayCritical(env, postMaxArray, postMaxArrayElems, 0);
	}

	return (*env)->CallStaticObjectMethod(env, gcBean, callBackID,
			(jlong)gcInfo->index, (jlong)gcInfo->startTime, (jlong)gcInfo->endTime,
			initialArray,
			preUsedArray,
			preCommittedArray,
			preMaxArray,
			postUsedArray,
			postCommittedArray,
			postMaxArray);
fail2:
	if (NULL != initialArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, initialArray, initialArrayElems, 0);
	}
	if (NULL != preUsedArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, preUsedArray, preUsedArrayElems, 0);
	}
	if (NULL != preCommittedArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, preCommittedArray, preCommittedArrayElems, 0);
	}
	if (NULL != preMaxArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, preMaxArray, preMaxArrayElems, 0);
	}
	if (NULL != postUsedArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, postUsedArray, postUsedArrayElems, 0);
	}
	if (NULL != postCommittedArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, postCommittedArray, postCommittedArrayElems, 0);
	}
	if (NULL != postMaxArrayElems) {
		(*env)->ReleasePrimitiveArrayCritical(env, postMaxArray, postMaxArrayElems, 0);
	}
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
