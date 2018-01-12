
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



jbyte JNICALL Java_JniArgTests_nativeBBrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBrB", 2, arg2, test_jbyte[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBBBBrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrB", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrB", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrB", 4, arg4, test_jbyte[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBSrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSrB", 2, arg2, test_jshort[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBSBSrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrB", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrB", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrB", 4, arg4, test_jshort[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBIrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIrB", 2, arg2, test_jint[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBIBIrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIBIrB", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrB", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeBIBIrB", 4, arg4, test_jint[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBJrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJrB", 2, arg2, test_jlong[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBJBJrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrB", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrB", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrB", 4, arg4, test_jlong[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBFrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFrB", 2, arg2, test_jfloat[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBFBFrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrB", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrB", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrB", 4, arg4, test_jfloat[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBDrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDrB", 2, arg2, test_jdouble[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeBDBDrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrB", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrB", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrB", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrB", 4, arg4, test_jdouble[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSBrB( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBrB", 2, arg2, test_jbyte[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSBSBrB( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrB", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrB", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrB", 4, arg4, test_jbyte[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSSrB( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSrB", 2, arg2, test_jshort[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSSSSrB( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrB", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrB", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrB", 4, arg4, test_jshort[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSIrB( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSIrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSIrB", 2, arg2, test_jint[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSISIrB( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSISIrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSISIrB", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSISIrB", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeSISIrB", 4, arg4, test_jint[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSJrB( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJrB", 2, arg2, test_jlong[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSJSJrB( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrB", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrB", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrB", 4, arg4, test_jlong[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSFrB( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFrB", 2, arg2, test_jfloat[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSFSFrB( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrB", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrB", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrB", 4, arg4, test_jfloat[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSDrB( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDrB", 2, arg2, test_jdouble[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeSDSDrB( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrB", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrB", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrB", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrB", 4, arg4, test_jdouble[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIBrB( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBrB", 2, arg2, test_jbyte[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIBIBrB( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBIBrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrB", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIBIBrB", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrB", 4, arg4, test_jbyte[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeISrB( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISrB", 2, arg2, test_jshort[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeISISrB( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISISrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISISrB", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeISISrB", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeISISrB", 4, arg4, test_jshort[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIIrB( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIrB", 2, arg2, test_jint[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIIIIrB( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIIIrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIIIrB", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIIIIrB", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIIIIrB", 4, arg4, test_jint[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIJrB( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJrB", 2, arg2, test_jlong[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIJIJrB( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJIJrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrB", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIJIJrB", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrB", 4, arg4, test_jlong[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIFrB( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFrB", 2, arg2, test_jfloat[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIFIFrB( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFIFrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrB", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIFIFrB", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrB", 4, arg4, test_jfloat[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIDrB( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDrB", 2, arg2, test_jdouble[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeIDIDrB( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDIDrB", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrB", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIDIDrB", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrB", 4, arg4, test_jdouble[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJBrB( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBrB", 2, arg2, test_jbyte[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJBJBrB( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrB", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrB", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrB", 4, arg4, test_jbyte[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJSrB( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSrB", 2, arg2, test_jshort[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJSJSrB( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrB", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrB", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrB", 4, arg4, test_jshort[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJIrB( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIrB", 2, arg2, test_jint[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJIJIrB( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIJIrB", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrB", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeJIJIrB", 4, arg4, test_jint[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJJrB( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJrB", 2, arg2, test_jlong[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJJJJrB( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrB", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrB", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrB", 4, arg4, test_jlong[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJFrB( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFrB", 2, arg2, test_jfloat[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJFJFrB( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrB", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrB", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrB", 4, arg4, test_jfloat[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJDrB( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDrB", 2, arg2, test_jdouble[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeJDJDrB( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrB", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrB", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrB", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrB", 4, arg4, test_jdouble[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFBrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBrB", 2, arg2, test_jbyte[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFBFBrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrB", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrB", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrB", 4, arg4, test_jbyte[4]);
	}
	return test_jbyte[0];
}





