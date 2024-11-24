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

extern "C" {

void JNICALL
Java_jdk_jfr_internal_JVM_markChunkFinal(JNIEnv *env, jobject obj)
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
Java_jdk_jfr_internal_JVM_setMethodSamplingPeriod(JNIEnv *env, jobject obj, jlong type, jlong periodMillis)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setThrottle(JNIEnv *env, jobject obj, jlong eventTypeId, jlong eventSampleSize, jlong period_ms)
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
#if JAVA_SPEC_VERSION == 17
Java_jdk_jfr_internal_JVM_isExcluded(JNIEnv *env, jobject obj, jobject thread)
#else /* JAVA_SPEC_VERSION == 17 */
Java_jdk_jfr_internal_JVM_isExcluded__Ljava_lang_Thread_2(JNIEnv *env, jobject obj, jobject thread)
#endif /* JAVA_SPEC_VERSION == 17 */
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

jlong JNICALL
Java_jdk_jfr_internal_JVM_getTypeId__Ljava_lang_String_2(JNIEnv *env, jobject obj, jstring name)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_flush__(JNIEnv *env, jclass clazz)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_emitDataLoss(JNIEnv *env, jclass clazz, jlong bytes)
{
	// TODO: implementation
}

} /* extern "C" */
