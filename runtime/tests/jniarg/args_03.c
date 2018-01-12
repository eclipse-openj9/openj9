
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



jshort JNICALL Java_JniArgTests_nativeIIrS( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIrS", 2, arg2, test_jint[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIIIIrS( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIIIrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIIIrS", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIIIIrS", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIIIIrS", 4, arg4, test_jint[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIJrS( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJrS", 2, arg2, test_jlong[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIJIJrS( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJIJrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrS", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIJIJrS", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrS", 4, arg4, test_jlong[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIFrS( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFrS", 2, arg2, test_jfloat[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIFIFrS( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFIFrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrS", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIFIFrS", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrS", 4, arg4, test_jfloat[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIDrS( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDrS", 2, arg2, test_jdouble[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIDIDrS( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDIDrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrS", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIDIDrS", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrS", 4, arg4, test_jdouble[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJBrS( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBrS", 2, arg2, test_jbyte[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJBJBrS( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrS", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrS", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrS", 4, arg4, test_jbyte[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJSrS( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSrS", 2, arg2, test_jshort[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJSJSrS( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrS", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrS", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrS", 4, arg4, test_jshort[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJIrS( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIrS", 2, arg2, test_jint[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJIJIrS( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIJIrS", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrS", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeJIJIrS", 4, arg4, test_jint[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJJrS( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJrS", 2, arg2, test_jlong[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJJJJrS( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrS", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrS", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrS", 4, arg4, test_jlong[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJFrS( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFrS", 2, arg2, test_jfloat[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJFJFrS( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrS", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrS", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrS", 4, arg4, test_jfloat[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJDrS( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDrS", 2, arg2, test_jdouble[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeJDJDrS( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrS", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrS", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrS", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrS", 4, arg4, test_jdouble[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFBrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBrS", 2, arg2, test_jbyte[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFBFBrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrS", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrS", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrS", 4, arg4, test_jbyte[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFSrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSrS", 2, arg2, test_jshort[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFSFSrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrS", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrS", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrS", 4, arg4, test_jshort[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFIrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIrS", 2, arg2, test_jint[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFIFIrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIFIrS", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrS", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeFIFIrS", 4, arg4, test_jint[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFJrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJrS", 2, arg2, test_jlong[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFJFJrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrS", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrS", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrS", 4, arg4, test_jlong[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFFrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFrS", 2, arg2, test_jfloat[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFFFFrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrS", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrS", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrS", 4, arg4, test_jfloat[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFDrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDrS", 2, arg2, test_jdouble[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeFDFDrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrS", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrS", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrS", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrS", 4, arg4, test_jdouble[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDBrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBrS", 2, arg2, test_jbyte[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDBDBrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrS", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrS", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrS", 4, arg4, test_jbyte[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDSrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSrS", 2, arg2, test_jshort[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDSDSrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrS", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrS", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrS", 4, arg4, test_jshort[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDIrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIrS", 2, arg2, test_jint[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDIDIrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIDIrS", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrS", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeDIDIrS", 4, arg4, test_jint[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDJrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJrS", 2, arg2, test_jlong[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDJDJrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrS", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrS", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrS", 4, arg4, test_jlong[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDFrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFrS", 2, arg2, test_jfloat[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDFDFrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrS", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrS", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrS", 4, arg4, test_jfloat[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDDrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDrS", 2, arg2, test_jdouble[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeDDDDrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrS", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrS", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrS", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrS", 4, arg4, test_jdouble[4]);
	}
	return test_jshort[0];
}

jint JNICALL Java_JniArgTests_nativeBBrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBrI", 2, arg2, test_jbyte[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBBBBrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrI", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrI", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrI", 4, arg4, test_jbyte[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBSrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSrI", 2, arg2, test_jshort[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBSBSrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrI", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrI", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrI", 4, arg4, test_jshort[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBIrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIrI", 2, arg2, test_jint[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeBIBIrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrI", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIBIrI", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrI", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeBIBIrI", 4, arg4, test_jint[4]);
	}
	return test_jint[0];
}





