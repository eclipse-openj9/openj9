/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include <jni.h>
#include "bcverify_api.h"

typedef I_32 (*nameCheckFunction)(J9CfrConstantPoolInfo * info);


/**
 * Common helper function to invoke one of the checkName()/checkClassName() validators
 * for UTF8 content.
 * @param env
 * @param clazz
 * @param name
 * @param checkFunction The validator function to exercise.
 * @return The value to return from the native.
 */
static jlong nameCheck(JNIEnv *env, jclass clazz, jstring name, nameCheckFunction checkFunction)
{
	const char* utfBytes;
	jsize nbyte;
	jboolean isCopy;
	J9CfrConstantPoolInfo cpInfo;
	IDATA rcCheck;
	
	if (NULL == name) {
		return -__LINE__;
	}
	
	nbyte = (*env)->GetStringUTFLength(env, name);
	utfBytes = (*env)->GetStringUTFChars(env, name, &isCopy);
	if (NULL == utfBytes) {
		return -__LINE__;
	}
	
	memset(&cpInfo, 0, sizeof(cpInfo));
	cpInfo.slot1 = nbyte;
	cpInfo.bytes = (U_8*)utfBytes;
	rcCheck = checkFunction(&cpInfo);

	(*env)->ReleaseStringUTFChars(env, name, utfBytes);
	return rcCheck;
}


/*
 * Class:     com_ibm_j9_test_bcverify_TestNatives
 * Method:    bcvCheckName
 * Signature: (Ljava/lang/String;)J
 */
jlong JNICALL
Java_com_ibm_j9_test_bcverify_TestNatives_bcvCheckName(JNIEnv *env, jclass clazz, jstring name)
{
	return nameCheck(env, clazz, name, bcvCheckName);
}


/*
 * Class:     com_ibm_j9_test_bcverify_TestNatives
 * Method:    bcvCheckClassName
 * Signature: (Ljava/lang/String;)J
 */
jlong JNICALL
Java_com_ibm_j9_test_bcverify_TestNatives_bcvCheckClassName(JNIEnv *env, jclass clazz, jstring name)
{
	return nameCheck(env, clazz, name, bcvCheckClassName);
}

/*
 * Class:     com_ibm_j9_test_bcverify_TestNatives
 * Method:    bcvCheckMethodName
 * Signature: (Ljava/lang/String;)J
 */
jlong JNICALL
Java_com_ibm_j9_test_bcverify_TestNatives_bcvCheckMethodName(JNIEnv *env, jclass clazz, jstring name)
{
	return nameCheck(env, clazz, name, bcvCheckMethodName);
}
