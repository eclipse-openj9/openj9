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

#include "j9.h"
#include "cfr.h"
#include "bcutil_api.h"
#include <jni.h>
#include <stdlib.h>

/*
 * Class:     com_ibm_j9_test_bcutil_TestNatives
 * Method:    verifyCanonisizeAndCopyUTF8
 * Signature: ([B[BI)I
 */
jint JNICALL
Java_com_ibm_j9_test_bcutil_TestNatives_verifyCanonisizeAndCopyUTF8(JNIEnv *env, jclass class, jintArray destArray, jintArray srcArray, jint length)
{
	int i;
	U_8 *src;
	U_8 *dest;
	jint result = -1;
	jint *jSrc = (*env)->GetIntArrayElements(env, srcArray, NULL);
	jint *jDest = (*env)->GetIntArrayElements(env, destArray, NULL);

	src = malloc(length);
	if (!src) {
		return -1;
	}
	dest = malloc(length);
	if (!dest) {
		free(src);
		return -1;
	}

	for (i =0; i < length; i++) {
		src[i] = (char) jSrc[i];
	}

	result = j9bcutil_verifyCanonisizeAndCopyUTF8(dest, src, length);

	for (i =0; i < length; i++) {
		jDest[i] = dest[i];
	}

	(*env)->ReleaseIntArrayElements(env, destArray, jDest, 0);
	(*env)->ReleaseIntArrayElements(env, srcArray, jSrc, JNI_ABORT);

	free(src);
	free(dest);

	return result;
}



