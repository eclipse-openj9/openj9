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
Java_jdk_jfr_internal_JVM_beginRecording(JNIEnv *env, jobject obj)
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
Java_jdk_jfr_internal_JVM_emitEvent(JNIEnv *env, jobject obj, jlong eventTypeId, jlong timestamp, jlong when)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_endRecording(JNIEnv *env, jobject obj)
{
	// TODO: implementation
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

jlong JNICALL
Java_jdk_jfr_internal_JVM_getClassIdNonIntrinsic(JNIEnv *env, jclass cls, jclass clazz)
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

void JNICALL
Java_jdk_jfr_internal_JVM_log(JNIEnv *env, jclass cls, jint tagSetId, jint level, jstring message)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_subscribeLogLevel(JNIEnv *env, jclass cls, jobject lt, jint tagSetId)
{
	// TODO: implementation
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
Java_jdk_jfr_internal_JVM_setMethodSamplingInterval(JNIEnv *env, jobject obj, jlong type, jlong intervalMillis)
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
Java_jdk_jfr_internal_JVM_setSampleThreads(JNIEnv *env, jobject obj, jboolean sampleThreads)
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
	return JNI_FALSE;
}

jdouble JNICALL
Java_jdk_jfr_internal_JVM_getTimeConversionFactor(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return 0.0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTypeId(JNIEnv *env, jobject obj, jclass clazz)
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

jboolean JNICALL
Java_jdk_jfr_internal_JVM_flush(JNIEnv *env, jclass cls, jobject writer, jint uncommittedSize, jint requestedSize)
{
	// TODO: implementation
	return JNI_FALSE;
}

void JNICALL
Java_jdk_jfr_internal_JVM_setRepositoryLocation(JNIEnv *env, jobject obj, jstring dirText)
{
	// TODO: implementation
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

void JNICALL
Java_jdk_jfr_internal_JVM_emitOldObjectSamples(JNIEnv *env, jobject obj, jlong cutoff, jboolean emitAll)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_shouldRotateDisk(JNIEnv *env, jobject obj)
{
	// TODO: implementation
	return JNI_FALSE;
}

}
