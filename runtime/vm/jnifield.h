/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef JNIFIELDSUP_H_
#define JNIFIELDSUP_H_

#include "j9cfg.h"
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Refer to the JNI documentation.
 */

jboolean JNICALL getBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID);
jbyte JNICALL getByteField(JNIEnv *env, jobject obj, jfieldID fieldID);
jshort JNICALL getShortField(JNIEnv *env, jobject obj, jfieldID fieldID);
jchar JNICALL getCharField(JNIEnv *env, jobject obj, jfieldID fieldID);
jint JNICALL getIntField(JNIEnv *env, jobject obj, jfieldID fieldID);
jlong JNICALL getLongField(JNIEnv *env, jobject obj, jfieldID fieldID);

void JNICALL setByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte value);
void JNICALL setBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean value);
void JNICALL setCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar value);
void JNICALL setShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort value);
void JNICALL setIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint value);
void JNICALL setLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong value);

#ifdef J9VM_INTERP_FLOAT_SUPPORT
jfloat JNICALL getFloatField(JNIEnv *env, jobject obj, jfieldID fieldID);
jdouble JNICALL getDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID);

void JNICALL setFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat value);
void JNICALL setDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble value);
#endif /* J9VM_INTERP_FLOAT_SUPPORT */

void JNICALL setObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject valueRef);
jobject JNICALL getObjectField(JNIEnv *env, jobject obj, jfieldID fieldID);

void JNICALL setObjectArrayElement(JNIEnv *env, jobjectArray arrayRef, jsize index, jobject valueRef);
jobject JNICALL getObjectArrayElement(JNIEnv *env, jobjectArray arrayRef, jsize index);

#ifdef __cplusplus
}
#endif

#endif /* JNIFIELDSUP_H_ */
