/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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

#include "j9.h"
#include "jni.h"

#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"

extern "C" {

/* Make sure these logging levels lineup with jdk/jfr/internal/LogLevel.java */
#define LOG_LEVEL_TRACE 1
#define LOG_LEVEL_DEBUG 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_WARN 4
#define LOG_LEVEL_ERROR 5

#define JFR_STRING_BUFFER 256

void JNICALL
Java_jdk_jfr_internal_JVM_registerNatives(JNIEnv *env, jclass cls)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_markChunkFinal(JNIEnv *env, jobject obj)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_beginRecording(JNIEnv *env, jobject obj)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isRecording(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_endRecording(JNIEnv *env, jobject obj)
{
	// TODO: implementation
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_counterTime(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_emitEvent(JNIEnv *env, jobject obj, jlong eventTypeId, jlong timestamp, jlong periodicType)
{
	// TODO: implementation
	return JNI_FALSE;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_getAllEventClasses(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return NULL;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getUnloadedEventClassCount(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getClassId(JNIEnv *env, jclass cls, jclass clazz)
{
	// TODO: implementation
	return 0;
}

jstring JNICALL
Java_jdk_jfr_internal_JVM_getPid(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return NULL;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getStackTraceId(JNIEnv *env, jobject obj, jint skipCount)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getThreadId(JNIEnv *env, jobject obj, jobject thread)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTicksFrequency(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return 0;
}

/**
 * TODO Note this is a draft implementation.
 */
static void
logJFRMessage(J9VMThread *currentThread, j9object_t stringMessage)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	char buf[JFR_STRING_BUFFER];

	J9UTF8* utf8Message = vmFuncs->copyStringToJ9UTF8WithMemAlloc(currentThread, stringMessage, J9_STR_NONE, "", 0, buf, JFR_STRING_BUFFER);
	if (NULL == utf8Message) {
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		j9tty_printf(PORTLIB, "%.*s\n", J9UTF8_LENGTH(utf8Message), J9UTF8_DATA(utf8Message));
		if (buf != (char*)utf8Message) {
			j9mem_free_memory(utf8Message);
		}
	}
}

/**
 * TODO Note this is a draft implementation.
 */
void JNICALL
Java_jdk_jfr_internal_JVM_log(JNIEnv *env, jclass cls, jint tagSetId, jint level, jstring message)
{
	J9VMThread *currentThread = (J9VMThread*) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == message) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t stringMessage = J9_JNI_UNWRAP_REFERENCE(message);
		logJFRMessage(currentThread, stringMessage);
	}

	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * TODO Note this is a draft implementation.
 */
void JNICALL
Java_jdk_jfr_internal_JVM_logEvent(JNIEnv *env, jclass cls, jint level, jobjectArray lines, jboolean system)
{
	J9VMThread *currentThread = (J9VMThread*) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == lines) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t stringArray = J9_JNI_UNWRAP_REFERENCE(lines);
		U_32 numOfLines = J9INDEXABLEOBJECT_SIZE(currentThread, stringArray);

		for (U_32 i = 0; i < numOfLines; i++) {
			logJFRMessage(currentThread, J9JAVAARRAYOFOBJECT_LOAD(currentThread, stringArray, i));
		}
	}

	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * Note this is a draft implementation.
 */
void JNICALL
Java_jdk_jfr_internal_JVM_subscribeLogLevel(JNIEnv *env, jclass cls, jobject lt, jint tagSetId)
{
	J9VMThread *currentThread = (J9VMThread*) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	j9object_t logTagInstance = J9_JNI_UNWRAP_REFERENCE(lt);
	J9Class *loggerClass = J9OBJECT_CLAZZ(currentThread, logTagInstance);
	IDATA tagSetLevelOffset = VM_VMHelpers::findinstanceFieldOffset(currentThread, loggerClass, "tagSetLevel", "I");

	if (-1 != tagSetLevelOffset) {
		MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);

		/* TODO for now we will use warn as the default, in the future we will parse -Xlog to determine actual level */
		objectAccessBarrier.inlineMixedObjectStoreI32(currentThread, logTagInstance, tagSetLevelOffset, LOG_LEVEL_WARN, TRUE);
	} else {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	}

	vmFuncs->internalExitVMToJNI(currentThread);
}

void JNICALL
Java_jdk_jfr_internal_JVM_retransformClasses(JNIEnv *env, jobject obj, jobjectArray classes)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setEnabled(JNIEnv *env, jobject obj, jlong eventTypeId, jboolean enabled)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setFileNotification(JNIEnv *env, jobject obj, jlong delta)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setGlobalBufferCount(JNIEnv *env, jobject obj, jlong count)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setGlobalBufferSize(JNIEnv *env, jobject obj, jlong size)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setMemorySize(JNIEnv *env, jobject obj, jlong size)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setMethodSamplingPeriod(JNIEnv *env, jobject obj, jlong type, jlong periodMillis)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setOutput(JNIEnv *env, jobject obj, jstring file)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setForceInstrumentation(JNIEnv *env, jobject obj, jboolean force)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setCompressedIntegers(JNIEnv *env, jobject obj, jboolean compressed)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setStackDepth(JNIEnv *env, jobject obj, jint depth)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setStackTraceEnabled(JNIEnv *env, jobject obj, jlong eventTypeId, jboolean enabled)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setThreadBufferSize(JNIEnv *env, jobject obj, jlong size)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setThreshold(JNIEnv *env, jobject obj, jlong eventTypeId, jlong ticks)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_storeMetadataDescriptor(JNIEnv *env, jobject obj, jbyteArray bytes)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_getAllowedToDoEventRetransforms(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_createJFR(JNIEnv *env, jobject obj, jboolean simulateFailure)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_destroyJFR(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isAvailable(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_TRUE;
}

jdouble JNICALL
Java_jdk_jfr_internal_JVM_getTimeConversionFactor(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return 0.0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTypeId__Ljava_lang_Class_2(JNIEnv *env, jobject obj, jclass clazz)
{
	// TODO: implementation
	return 0;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_getEventWriter(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return NULL;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_newEventWriter(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return NULL;
}

void JNICALL
Java_jdk_jfr_internal_JVM_flush__Ljdk_jfr_internal_event_EventWriter_2II(JNIEnv *env, jclass cls, jobject writer, jint uncommittedSize, jint requestedSize)
{
	// TODO: implementation
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_commit(JNIEnv *env, jclass cls, jlong nextPosition)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_flush__(JNIEnv *env, jobject obj)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setRepositoryLocation(JNIEnv *env, jobject obj, jstring dirText)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setDumpPath(JNIEnv *env, jobject obj, jstring dumpPathText)
{
	// TODO: implementation
}

jstring JNICALL
Java_jdk_jfr_internal_JVM_getDumpPath(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return NULL;
}

void JNICALL
Java_jdk_jfr_internal_JVM_abort(JNIEnv *env, jobject obj, jstring errorMsg)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_addStringConstant(JNIEnv *env, jclass cls, jlong id, jstring s)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_uncaughtException(JNIEnv *env, jobject obj, jobject thread, jobject t)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setCutoff(JNIEnv *env, jobject obj, jlong eventTypeId, jlong cutoffTicks)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setThrottle(JNIEnv *env, jobject obj, jlong eventTypeId, jlong eventSampleSize, jlong period_ms)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_emitOldObjectSamples(JNIEnv *env, jobject obj, jlong cutoff, jboolean emitAll, jboolean skipBFS)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_shouldRotateDisk(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_exclude(JNIEnv *env, jobject obj, jobject thread)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_include(JNIEnv *env, jobject obj, jobject thread)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isExcluded(JNIEnv *env, jobject obj, jobject thread)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isExcluded__Ljava_lang_Class_2(JNIEnv *env, jobject obj, jclass eventClass)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isInstrumented(JNIEnv *env, jobject obj, jclass eventClass)
{
	// TODO: implementation
	return JNI_FALSE;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getChunkStartNanos(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return 0;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setConfiguration(JNIEnv *env, jobject obj, jclass eventClass, jobject configuration)
{
	// TODO: implementation
	return JNI_FALSE;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_getConfiguration(JNIEnv *env, jobject obj, jclass eventClass)
{
	// TODO: implementation
	return NULL;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTypeId__Ljava_lang_String_2(JNIEnv *env, jobject obj, jstring name)
{
	// TODO: implementation
	return 0;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isContainerized(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_FALSE;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_hostTotalMemory(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_emitDataLoss(JNIEnv *env, jclass cls, jlong bytes)
{
	// TODO: implementation
}

}
