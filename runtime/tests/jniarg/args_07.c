
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



jfloat JNICALL Java_JniArgTests_nativeSBrF( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBrF", 2, arg2, test_jbyte[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSBSBrF( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrF", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrF", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrF", 4, arg4, test_jbyte[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSSrF( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSrF", 2, arg2, test_jshort[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSSSSrF( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrF", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrF", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrF", 4, arg4, test_jshort[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSIrF( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSIrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSIrF", 2, arg2, test_jint[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSISIrF( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSISIrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSISIrF", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSISIrF", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeSISIrF", 4, arg4, test_jint[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSJrF( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJrF", 2, arg2, test_jlong[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSJSJrF( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrF", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrF", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrF", 4, arg4, test_jlong[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSFrF( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFrF", 2, arg2, test_jfloat[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSFSFrF( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrF", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrF", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrF", 4, arg4, test_jfloat[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSDrF( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDrF", 2, arg2, test_jdouble[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeSDSDrF( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrF", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrF", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrF", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrF", 4, arg4, test_jdouble[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIBrF( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBrF", 2, arg2, test_jbyte[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIBIBrF( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBIBrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrF", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIBIBrF", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrF", 4, arg4, test_jbyte[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeISrF( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISrF", 2, arg2, test_jshort[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeISISrF( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISISrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISISrF", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeISISrF", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeISISrF", 4, arg4, test_jshort[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIIrF( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIrF", 2, arg2, test_jint[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIIIIrF( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIIIrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIIIrF", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIIIIrF", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIIIIrF", 4, arg4, test_jint[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIJrF( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJrF", 2, arg2, test_jlong[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIJIJrF( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJIJrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrF", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIJIJrF", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrF", 4, arg4, test_jlong[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIFrF( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFrF", 2, arg2, test_jfloat[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIFIFrF( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFIFrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrF", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIFIFrF", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrF", 4, arg4, test_jfloat[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIDrF( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDrF", 2, arg2, test_jdouble[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeIDIDrF( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDIDrF", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrF", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIDIDrF", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrF", 4, arg4, test_jdouble[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJBrF( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBrF", 2, arg2, test_jbyte[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJBJBrF( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrF", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrF", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrF", 4, arg4, test_jbyte[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJSrF( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSrF", 2, arg2, test_jshort[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJSJSrF( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrF", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrF", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrF", 4, arg4, test_jshort[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJIrF( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIrF", 2, arg2, test_jint[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJIJIrF( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIJIrF", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrF", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeJIJIrF", 4, arg4, test_jint[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJJrF( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJrF", 2, arg2, test_jlong[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJJJJrF( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrF", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrF", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrF", 4, arg4, test_jlong[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJFrF( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFrF", 2, arg2, test_jfloat[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJFJFrF( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrF", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrF", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrF", 4, arg4, test_jfloat[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJDrF( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDrF", 2, arg2, test_jdouble[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeJDJDrF( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrF", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrF", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrF", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrF", 4, arg4, test_jdouble[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFBrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBrF", 2, arg2, test_jbyte[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFBFBrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrF", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrF", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrF", 4, arg4, test_jbyte[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFSrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSrF", 2, arg2, test_jshort[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFSFSrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrF", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrF", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrF", 4, arg4, test_jshort[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFIrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIrF", 2, arg2, test_jint[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFIFIrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIFIrF", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrF", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeFIFIrF", 4, arg4, test_jint[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFJrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJrF", 2, arg2, test_jlong[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFJFJrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrF", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrF", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrF", 4, arg4, test_jlong[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFFrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFrF", 2, arg2, test_jfloat[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFFFFrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrF", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrF", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrF", 4, arg4, test_jfloat[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFDrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDrF", 2, arg2, test_jdouble[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeFDFDrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrF", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrF", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrF", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrF", 4, arg4, test_jdouble[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeDBrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBrF", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBrF", 2, arg2, test_jbyte[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeDBDBrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrF", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrF", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrF", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrF", 4, arg4, test_jbyte[4]);
	}
	return test_jfloat[0];
}





