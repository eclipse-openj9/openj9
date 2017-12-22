
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



jlong JNICALL Java_JniArgTests_nativeIDrJ( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDrJ", 2, arg2, test_jdouble[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIDIDrJ( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIDIDrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrJ", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIDIDrJ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeIDIDrJ", 4, arg4, test_jdouble[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJBrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBrJ", 2, arg2, test_jbyte[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJBJBrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrJ", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJBJBrJ", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeJBJBrJ", 4, arg4, test_jbyte[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJSrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSrJ", 2, arg2, test_jshort[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJSJSrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrJ", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJSJSrJ", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeJSJSrJ", 4, arg4, test_jshort[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJIrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIrJ", 2, arg2, test_jint[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJIJIrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeJIJIrJ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJIJIrJ", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeJIJIrJ", 4, arg4, test_jint[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJJrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJrJ", 2, arg2, test_jlong[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJJJJrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrJ", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrJ", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeJJJJrJ", 4, arg4, test_jlong[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJFrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFrJ", 2, arg2, test_jfloat[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJFJFrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrJ", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJFJFrJ", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeJFJFrJ", 4, arg4, test_jfloat[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJDrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDrJ", 2, arg2, test_jdouble[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeJDJDrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jlong[1]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrJ", 1, arg1, test_jlong[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrJ", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jlong[3]) {
		cFailure_jlong(PORTLIB, "nativeJDJDrJ", 3, arg3, test_jlong[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeJDJDrJ", 4, arg4, test_jdouble[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFBrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBrJ", 2, arg2, test_jbyte[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFBFBrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrJ", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFBFBrJ", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeFBFBrJ", 4, arg4, test_jbyte[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFSrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSrJ", 2, arg2, test_jshort[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFSFSrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrJ", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrJ", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrJ", 4, arg4, test_jshort[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFIrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIrJ", 2, arg2, test_jint[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFIFIrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIFIrJ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrJ", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeFIFIrJ", 4, arg4, test_jint[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFJrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJrJ", 2, arg2, test_jlong[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFJFJrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrJ", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrJ", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrJ", 4, arg4, test_jlong[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFFrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFrJ", 2, arg2, test_jfloat[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFFFFrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrJ", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrJ", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrJ", 4, arg4, test_jfloat[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFDrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDrJ", 2, arg2, test_jdouble[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeFDFDrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrJ", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrJ", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrJ", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrJ", 4, arg4, test_jdouble[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDBrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBrJ", 2, arg2, test_jbyte[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDBDBrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrJ", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrJ", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrJ", 4, arg4, test_jbyte[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDSrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSrJ", 2, arg2, test_jshort[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDSDSrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrJ", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrJ", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrJ", 4, arg4, test_jshort[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDIrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIrJ", 2, arg2, test_jint[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDIDIrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIDIrJ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrJ", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeDIDIrJ", 4, arg4, test_jint[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDJrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJrJ", 2, arg2, test_jlong[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDJDJrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrJ", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrJ", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrJ", 4, arg4, test_jlong[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDFrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFrJ", 2, arg2, test_jfloat[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDFDFrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrJ", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrJ", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrJ", 4, arg4, test_jfloat[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDDrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDrJ", 2, arg2, test_jdouble[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeDDDDrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrJ", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrJ", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrJ", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrJ", 4, arg4, test_jdouble[4]);
	}
	return test_jlong[0];
}

jfloat JNICALL Java_JniArgTests_nativeBBrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBrF", 2, arg2, test_jbyte[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBBBBrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrF", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrF", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrF", 4, arg4, test_jbyte[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBSrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSrF", 2, arg2, test_jshort[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBSBSrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrF", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrF", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrF", 4, arg4, test_jshort[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBIrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIrF", 2, arg2, test_jint[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBIBIrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIBIrF", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrF", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeBIBIrF", 4, arg4, test_jint[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBJrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJrF", 2, arg2, test_jlong[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBJBJrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrF", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrF", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrF", 4, arg4, test_jlong[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBFrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFrF", 2, arg2, test_jfloat[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBFBFrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrF", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrF", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrF", 4, arg4, test_jfloat[4]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBDrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDrF", 2, arg2, test_jdouble[2]);
	}
	return test_jfloat[0];
}

jfloat JNICALL Java_JniArgTests_nativeBDBDrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrF", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrF", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrF", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrF", 4, arg4, test_jdouble[4]);
	}
	return test_jfloat[0];
}





