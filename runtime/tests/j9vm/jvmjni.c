/*******************************************************************************
 * Copyright (c) 2002, 2019 IBM Corp. and others
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

/* Avoid renaming malloc/free */
#define LAUNCHERS
#include "../j9vm/jvm.h"
#include "jni.h"


jarray JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetSystemPackages(JNIEnv* env, jclass unused)
{
	return JVM_GetSystemPackages(env);
}



jstring JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetSystemPackage(JNIEnv* env, jclass rcvr, jstring pkgName)
{
	return JVM_GetSystemPackage(env, pkgName);
}



JNIEXPORT jint JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_Close(JNIEnv* env, jclass clazz, jint descriptor)
{
	return JVM_Close(descriptor);
}



JNIEXPORT jint JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_UcsOpen(JNIEnv* env, jclass clazz, jstring filename, jint flags, jint mode)
{
	const jchar *filenameChars = (*env)->GetStringChars(env, filename, NULL);
	jint result = JVM_UcsOpen(filenameChars, flags, mode);
	(*env)->ReleaseStringChars(env, filename, filenameChars);
	return result;
}



JNIEXPORT jint JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_Write(JNIEnv* env, jclass clazz, jint descriptor, jstring aString)
{
	jsize length = (*env)->GetStringUTFLength(env, aString);
	const char *buffer = (*env)->GetStringUTFChars(env, aString, NULL);
	jint result = JVM_Write(descriptor, (const char *) buffer, (jint) length);
	(*env)->ReleaseStringUTFChars(env, aString, buffer);
	return result;
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GCNoCompact(JNIEnv* env, jclass unused)
{
	JVM_GCNoCompact();
}



jlong JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_MaxObjectInspectionAge(JNIEnv* env, jclass unused)
{
	return JVM_MaxObjectInspectionAge();
}



jint JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetArrayLength(JNIEnv * env, jclass unused, jobject array)
{
	return JVM_GetArrayLength(env, array);
}



jobject JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetArrayElement(env, array, index);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jobject element)
{
	JVM_SetArrayElement(env, array, index, element);
}



jlong JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetPrimitiveArrayElementWCodeParam(JNIEnv * env, jclass unused, jobject array, jint index, jint wcode)
{
	I_64 val = 0;

	switch (wcode) {
	case 4:
		return JVM_GetPrimitiveArrayElement(env, array, index, wcode).z;
		break;
	case 5:
		return JVM_GetPrimitiveArrayElement(env, array, index, wcode).c;
		break;
	case 6:
		val = (I_64) JVM_GetPrimitiveArrayElement(env, array, index, wcode).f;
		return val;
		break;
	case 7:
		val = (I_64) JVM_GetPrimitiveArrayElement(env, array, index, wcode).d;
		return val;
		break;
	case 8:
		return JVM_GetPrimitiveArrayElement(env, array, index, wcode).b;
		break;
	case 9:
		return JVM_GetPrimitiveArrayElement(env, array, index, wcode).s;
		break;
	case 10:
		return JVM_GetPrimitiveArrayElement(env, array, index, wcode).i;
		break;
	case 11:
		return JVM_GetPrimitiveArrayElement(env, array, index, wcode).j;
		break;
	}
	return 0;
}


jlong JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetLongArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 11).j;
}



jboolean JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetBooleanArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 4).z;
}



jint JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetIntArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 10).i;
}



jshort JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetShortArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 9).s;
}



jbyte JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetByteArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 8).b;
}



jchar JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetCharArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 5).c;
}



jfloat JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetFloatArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 6).f;
}



jdouble JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetDoubleArrayElement(JNIEnv * env, jclass unused, jobject array, jint index)
{
	return JVM_GetPrimitiveArrayElement(env, array, index, 7).d;
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetPrimitiveArrayElementWCodeParam(JNIEnv * env, jclass unused, jobject array, jint index, jlong value, jchar vCode)
{
	jvalue element;

	element.j = 0;

	switch (vCode) {
	case 11:
		element.j = (jlong) value;
		break;
	case 4:
		element.z = (jboolean) value;
		break;
	case 8:
		element.b = (jbyte) value;
		break;
	case 5:
		element.c = (jchar) value;
		break;
	case 9:
		element.s = (jshort) value;
		break;
	case 10:
		element.i = (jint) value;
		break;
	case 6:
		element.f = (jfloat) value;
		break;
	case 7:
		element.d = (jdouble) value;
		break;
	}

	JVM_SetPrimitiveArrayElement(env, array, index, element,(unsigned char) vCode);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetLongArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jlong value)
{
	jvalue element;

	element.j = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 11);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetBooleanArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jboolean value)
{
	jvalue element;

	element.j = 0;
	element.z = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 4);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetIntArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jint value)
{
	jvalue element;

	element.j = 0;
	element.i = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 10);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetShortArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jshort value)
{
	jvalue element;

	element.j = 0;
	element.s = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element,9);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetByteArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jbyte value)
{
	jvalue element;

	element.j = 0;
	element.b = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 8);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetCharArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jchar value)
{
	jvalue element;

	element.j = 0;
	element.c = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 5);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetFloatArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jfloat value)
{
	jvalue element;

	element.j = 0;
	element.f = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 6);
}



void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_SetDoubleArrayElement(JNIEnv * env, jclass unused, jobject array, jint index, jdouble value)
{
	jvalue element;

	element.j = 0;
	element.d = value;

	JVM_SetPrimitiveArrayElement(env, array, index, element, 7);
}

void JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_ArrayCopy(JNIEnv * env, jclass unused, jobject src, jint src_pos, jobject dst, jint dst_pos, jint length)
{
	JVM_ArrayCopy(env, unused, src, src_pos, dst, dst_pos, length);
}

#if JAVA_SPEC_VERSION >= 9
jlong JNICALL
Java_com_ibm_oti_jvmtests_SupportJVM_GetNanoTimeAdjustment(JNIEnv *env, jclass clazz, jlong offset_seconds)
{
	return JVM_GetNanoTimeAdjustment(env, clazz, offset_seconds);
}
#endif /* JAVA_SPEC_VERSION >= 9 */
