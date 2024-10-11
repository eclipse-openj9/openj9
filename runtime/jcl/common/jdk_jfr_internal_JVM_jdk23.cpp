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

#include "jni.h"

extern "C" {

void JNICALL
Java_jdk_jfr_internal_JVM_registerNatives(JNIEnv *env, jclass cls)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_markChunkFinal(JNIEnv *env, jclass cls)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_beginRecording(JNIEnv *env, jclass cls)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isRecording(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_endRecording(JNIEnv *env, jclass cls)
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
Java_jdk_jfr_internal_JVM_emitEvent(JNIEnv *env, jclass cls, jlong eventTypeId, jlong timestamp, jlong periodicType)
{
	// TODO: implementation
	return JNI_FALSE;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_getAllEventClasses(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return NULL;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getUnloadedEventClassCount(JNIEnv *env, jclass cls)
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
Java_jdk_jfr_internal_JVM_getPid(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return NULL;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getStackTraceId(JNIEnv *env, jclass cls, jint skipCount, jlong stackFilerId)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getThreadId(JNIEnv *env, jclass cls, jobject thread)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTicksFrequency(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_nanosNow(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_log(JNIEnv *env, jclass cls, jint tagSetId, jint level, jstring message)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_logEvent(JNIEnv *env, jclass cls, jint level, jobjectArray lines, jboolean system)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_subscribeLogLevel(JNIEnv *env, jclass cls, jobject lt, jint tagSetId)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_retransformClasses(JNIEnv *env, jclass cls, jobjectArray classes)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setEnabled(JNIEnv *env, jclass cls, jlong eventTypeId, jboolean enabled)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setFileNotification(JNIEnv *env, jclass cls, jlong delta)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setGlobalBufferCount(JNIEnv *env, jclass cls, jlong count)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setGlobalBufferSize(JNIEnv *env, jclass cls, jlong size)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setMemorySize(JNIEnv *env, jclass cls, jlong size)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setMethodSamplingPeriod(JNIEnv *env, jclass cls, jlong type, jlong periodMillis)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setOutput(JNIEnv *env, jclass cls, jstring file)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setForceInstrumentation(JNIEnv *env, jclass cls, jboolean force)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setCompressedIntegers(JNIEnv *env, jclass cls, jboolean compressed)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setStackDepth(JNIEnv *env, jclass cls, jint depth)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setStackTraceEnabled(JNIEnv *env, jclass cls, jlong eventTypeId, jboolean enabled)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setThreadBufferSize(JNIEnv *env, jclass cls, jlong size)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setThreshold(JNIEnv *env, jclass cls, jlong eventTypeId, jlong ticks)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_storeMetadataDescriptor(JNIEnv *env, jclass cls, jbyteArray bytes)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_getAllowedToDoEventRetransforms(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_createJFR(JNIEnv *env, jclass cls, jboolean simulateFailure)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_destroyJFR(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isAvailable(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return JNI_FALSE;
}

jdouble JNICALL
Java_jdk_jfr_internal_JVM_getTimeConversionFactor(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0.0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTypeId(JNIEnv *env, jclass cls, jclass clazz)
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
Java_jdk_jfr_internal_JVM_flush__(JNIEnv *env, jclass cls)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setRepositoryLocation(JNIEnv *env, jclass cls, jstring dirText)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setDumpPath(JNIEnv *env, jclass cls, jstring dumpPathText)
{
	// TODO: implementation
}

jstring JNICALL
Java_jdk_jfr_internal_JVM_getDumpPath(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return NULL;
}

void JNICALL
Java_jdk_jfr_internal_JVM_abort(JNIEnv *env, jclass cls, jstring errorMsg)
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
Java_jdk_jfr_internal_JVM_uncaughtException(JNIEnv *env, jclass cls, jobject thread, jobject t)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setCutoff(JNIEnv *env, jclass cls, jlong eventTypeId, jlong cutoffTicks)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setThrottle(JNIEnv *env, jclass cls, jlong eventTypeId, jlong eventSampleSize, jlong period_ms)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_emitOldObjectSamples(JNIEnv *env, jclass cls, jlong cutoff, jboolean emitAll, jboolean skipBFS)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_shouldRotateDisk(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_exclude(JNIEnv *env, jclass cls, jobject thread)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_include(JNIEnv *env, jclass cls, jobject thread)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isExcluded__Ljava_lang_Thread_2(JNIEnv *env, jclass cls, jobject thread)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isExcluded__Ljava_lang_Class_2(JNIEnv *env, jclass cls, jclass eventClass)
{
	// TODO: implementation
	return JNI_FALSE;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isInstrumented(JNIEnv *env, jclass cls, jclass eventClass)
{
	// TODO: implementation
	return JNI_FALSE;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getChunkStartNanos(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setConfiguration(JNIEnv *env, jclass cls, jclass eventClass, jobject configuration)
{
	// TODO: implementation
	return JNI_FALSE;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_getConfiguration(JNIEnv *env, jclass cls, jclass eventClass)
{
	// TODO: implementation
	return NULL;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTypeId__Ljava_lang_String_2(JNIEnv *env, jclass cls, jstring name)
{
	// TODO: implementation
	return 0;
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_isContainerized(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return JNI_FALSE;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_hostTotalMemory(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_hostTotalSwapMemory(JNIEnv *env, jclass cls)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_emitDataLoss(JNIEnv *env, jclass cls, jlong bytes)
{
	// TODO: implementation
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_registerStackFilter(JNIEnv *env, jclass cls, jobjectArray classes, jobjectArray methods)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_unregisterStackFilter(JNIEnv *env, jclass cls, jlong stackFilterId)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setMiscellaneous(JNIEnv *env, jclass cls, jlong eventTypeId, jlong value)
{
	// TODO: implementation
}

}
