/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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

#include "jcl.h"

/*
 * This is the minimum implementation of sun.misc.Perf natives to
 * make swingset run. See CMVC 157428 for detailed descriptions
 * of what these methods should do.
 */

/* public static native void registerNatives(); */
void JNICALL
Java_sun_misc_Perf_registerNatives(JNIEnv *env, jclass klass)
{
}

/* private native ByteBuffer attach(String user, int lvmid, int mode) throws IllegalArgumentException, IOException; */
jobject JNICALL
Java_sun_misc_Perf_attach(JNIEnv *env, jobject perf, jstring user, jint lvmid, jint mode)
{
	return NULL;
}

/* private native void detach(java.nio.ByteBuffer); */
jobject JNICALL
Java_sun_misc_Perf_detach(JNIEnv * env, jobject perf, jobject byteBuffer)
{
	return NULL;
}

/* public native ByteBuffer createLong(String name, int variability, int units, long value); */
jobject JNICALL
Java_sun_misc_Perf_createLong(JNIEnv *env, jobject perf, jstring name, jint variability, jint units, jlong value)
{
	jclass klass;
	jmethodID method;
	jobject result = NULL;

	klass = (*env)->FindClass(env, "java/nio/ByteBuffer");
	if (NULL != klass) {
		method = (*env)->GetStaticMethodID(env, klass, "allocateDirect", "(I)Ljava/nio/ByteBuffer;");
		if (NULL != method) {
			result = (*env)->CallStaticObjectMethod(env, klass, method, 8);
		}
	}

	return result;
}

/* public native ByteBuffer createByteArray(String name, int variability, int units, byte[] value, int maxLength); */
jobject JNICALL
Java_sun_misc_Perf_createByteArray(JNIEnv *env, jobject perf, jstring name, jint variability, jint units, jarray value, jint maxLength)
{
	return NULL;
}

/* public native long highResCounter(); */
jlong JNICALL
Java_sun_misc_Perf_highResCounter(JNIEnv *env, jobject perf)
{
	PORT_ACCESS_FROM_ENV(env);
	return j9time_hires_clock();
}

/* public native long highResFrequency(); */
jlong JNICALL
Java_sun_misc_Perf_highResFrequency(JNIEnv *env, jobject perf)
{
	PORT_ACCESS_FROM_ENV(env);
	return j9time_hires_frequency();
}

void
registerJdkInternalPerfPerfNatives(JNIEnv *env, jclass clazz) {
	/* clazz can't be null */
	JNINativeMethod natives[] = {
		{
			(char*)"createLong",
			(char*)"(Ljava/lang/String;IIJ)Ljava/nio/ByteBuffer;",
			(void *)&Java_sun_misc_Perf_createLong
		},
		{
			(char*)"createByteArray",
			(char*)"(Ljava/lang/String;II[BI)Ljava/nio/ByteBuffer;",
			(void *)&Java_sun_misc_Perf_createByteArray
		},
		{
			(char*)"attach",
			(char*)"(Ljava/lang/String;II)Ljava/nio/ByteBuffer;",
			(void *)&Java_sun_misc_Perf_attach
		},
		{
			(char*)"detach",
			(char*)"(Ljava/nio/ByteBuffer;)V",
			(void *)&Java_sun_misc_Perf_detach
		},
		{
			(char*)"highResCounter",
			(char*)"()J",
			(void *)&Java_sun_misc_Perf_highResCounter
		},
		{
			(char*)"highResFrequency",
			(char*)"()J",
			(void *)&Java_sun_misc_Perf_highResFrequency
		},
	};
	(*env)->RegisterNatives(env, clazz, natives, sizeof(natives)/sizeof(JNINativeMethod));
}

void JNICALL
Java_jdk_internal_perf_Perf_registerNatives(JNIEnv *env, jclass clazz)
{
	registerJdkInternalPerfPerfNatives(env, clazz);
}
