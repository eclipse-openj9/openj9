
/*******************************************************************************
 * Copyright (c) 1998, 2014 IBM Corp. and others
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

#include "jniargtests.h"



jint JNICALL Java_JniArgTests_nativeBJrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJrI", 2, arg2, test_jlong[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBJBJrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrI", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrI", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrI", 4, arg4, test_jlong[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBFrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFrI", 2, arg2, test_jfloat[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBFBFrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrI", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrI", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrI", 4, arg4, test_jfloat[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBDrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDrI", 2, arg2, test_jdouble[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBDBDrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrI", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrI", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrI", 4, arg4, test_jdouble[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSBrI( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBrI", 2, arg2, test_jbyte[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSBSBrI( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrI", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrI", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrI", 4, arg4, test_jbyte[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSSrI( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSrI", 2, arg2, test_jshort[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSSSSrI( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrI", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrI", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrI", 4, arg4, test_jshort[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSIrI( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSIrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSIrI", 2, arg2, test_jint[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSISIrI( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSISIrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSISIrI", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSISIrI", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeSISIrI", 4, arg4, test_jint[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSJrI( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJrI", 2, arg2, test_jlong[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSJSJrI( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrI", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrI", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrI", 4, arg4, test_jlong[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSFrI( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFrI", 2, arg2, test_jfloat[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSFSFrI( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrI", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrI", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrI", 4, arg4, test_jfloat[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSDrI( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDrI", 2, arg2, test_jdouble[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeSDSDrI( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrI", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrI", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrI", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrI", 4, arg4, test_jdouble[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIBrI( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBrI", 2, arg2, test_jbyte[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIBIBrI( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBIBrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrI", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIBIBrI", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrI", 4, arg4, test_jbyte[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeISrI( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISrI", 2, arg2, test_jshort[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeISISrI( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISISrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISISrI", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeISISrI", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeISISrI", 4, arg4, test_jshort[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIIrI( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIrI", 2, arg2, test_jint[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIIIIrI( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIIIrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIIIrI", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIIIIrI", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIIIIrI", 4, arg4, test_jint[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIJrI( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJrI", 2, arg2, test_jlong[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIJIJrI( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJIJrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrI", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIJIJrI", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrI", 4, arg4, test_jlong[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIFrI( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFrI", 2, arg2, test_jfloat[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIFIFrI( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFIFrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrI", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIFIFrI", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrI", 4, arg4, test_jfloat[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIDrI( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDrI", 2, arg2, test_jdouble[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeIDIDrI( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDIDrI", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrI", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIDIDrI", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrI", 4, arg4, test_jdouble[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJBrI( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBrI", 2, arg2, test_jbyte[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJBJBrI( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrI", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrI", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrI", 4, arg4, test_jbyte[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJSrI( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSrI", 2, arg2, test_jshort[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJSJSrI( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrI", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrI", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrI", 4, arg4, test_jshort[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJIrI( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIrI", 2, arg2, test_jint[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJIJIrI( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIJIrI", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrI", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeJIJIrI", 4, arg4, test_jint[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJJrI( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJrI", 2, arg2, test_jlong[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJJJJrI( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrI", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrI", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrI", 4, arg4, test_jlong[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJFrI( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFrI", 2, arg2, test_jfloat[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJFJFrI( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrI", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrI", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrI", 4, arg4, test_jfloat[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJDrI( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDrI", 2, arg2, test_jdouble[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeJDJDrI( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrI", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrI", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrI", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrI", 4, arg4, test_jdouble[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFBrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBrI", 2, arg2, test_jbyte[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFBFBrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrI", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrI", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrI", 4, arg4, test_jbyte[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFSrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSrI", 2, arg2, test_jshort[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFSFSrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrI", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrI", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrI", 4, arg4, test_jshort[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFIrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIrI", 2, arg2, test_jint[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFIFIrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIFIrI", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrI", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeFIFIrI", 4, arg4, test_jint[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFJrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJrI", 2, arg2, test_jlong[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFJFJrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrI", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrI", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrI", 4, arg4, test_jlong[4]);
	}
	return test_jint[0];
}





