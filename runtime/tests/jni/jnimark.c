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
/* Header for class com_ibm_j9_jnimark_Natives */


 static void
 throwException(JNIEnv *env, const char *name, const char *msg)
 {
     jclass cls = (*env)->FindClass(env, name);
     /* if cls is NULL, an exception has already been thrown */
     if (cls != NULL) {
         (*env)->ThrowNew(env, cls, msg);
     }
     /* free the local ref */
     (*env)->DeleteLocalRef(env, cls);
 }


/*
 * Class:     com_ibm_j9_jnimark_Natives
 * Method:    getByteArrayElements
 * Signature: ([B)I
 */
jint JNICALL
Java_com_ibm_j9_jnimark_Natives_getByteArrayElements(JNIEnv * env, jclass clazz, jbyteArray array)
{
	jbyte* elements;

	if (NULL == array) {
		throwException(env, "java/lang/IllegalArgumentException", "a byte array must be provided.");
		return -1;
	}

	elements = (*env)->GetByteArrayElements(env, array, NULL);
	if (NULL != elements) {
		(*env)->ReleaseByteArrayElements(env, array, elements, JNI_ABORT);
		return (*env)->GetArrayLength(env, array);
	} else {
		return 0;
	}
}
