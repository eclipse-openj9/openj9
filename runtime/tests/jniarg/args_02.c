
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



jbyte JNICALL Java_JniArgTests_nativeFSrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSrB", 2, arg2, test_jshort[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFSFSrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrB", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFSFSrB", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeFSFSrB", 4, arg4, test_jshort[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFIrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIrB", 2, arg2, test_jint[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFIFIrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeFIFIrB", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFIFIrB", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeFIFIrB", 4, arg4, test_jint[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFJrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJrB", 2, arg2, test_jlong[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFJFJrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrB", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFJFJrB", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeFJFJrB", 4, arg4, test_jlong[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFFrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFrB", 2, arg2, test_jfloat[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFFFFrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrB", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrB", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrB", 4, arg4, test_jfloat[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFDrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDrB", 2, arg2, test_jdouble[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeFDFDrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrB", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrB", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrB", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrB", 4, arg4, test_jdouble[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDBrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBrB", 2, arg2, test_jbyte[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDBDBrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrB", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrB", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrB", 4, arg4, test_jbyte[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDSrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSrB", 2, arg2, test_jshort[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDSDSrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrB", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrB", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrB", 4, arg4, test_jshort[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDIrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIrB", 2, arg2, test_jint[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDIDIrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIDIrB", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrB", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeDIDIrB", 4, arg4, test_jint[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDJrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJrB", 2, arg2, test_jlong[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDJDJrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrB", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrB", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrB", 4, arg4, test_jlong[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDFrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFrB", 2, arg2, test_jfloat[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDFDFrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrB", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrB", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrB", 4, arg4, test_jfloat[4]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDDrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDrB", 2, arg2, test_jdouble[2]);
	}
	return test_jbyte[0];
}

jbyte JNICALL Java_JniArgTests_nativeDDDDrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrB", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrB", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrB", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrB", 4, arg4, test_jdouble[4]);
	}
	return test_jbyte[0];
}

jshort JNICALL Java_JniArgTests_nativeBBrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBrS", 2, arg2, test_jbyte[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBBBBrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrS", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrS", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrS", 4, arg4, test_jbyte[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBSrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSrS", 2, arg2, test_jshort[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBSBSrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrS", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrS", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrS", 4, arg4, test_jshort[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBIrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIrS", 2, arg2, test_jint[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBIBIrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIBIrS", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrS", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeBIBIrS", 4, arg4, test_jint[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBJrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJrS", 2, arg2, test_jlong[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBJBJrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrS", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrS", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrS", 4, arg4, test_jlong[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBFrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFrS", 2, arg2, test_jfloat[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBFBFrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrS", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrS", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrS", 4, arg4, test_jfloat[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBDrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDrS", 2, arg2, test_jdouble[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeBDBDrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrS", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrS", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrS", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrS", 4, arg4, test_jdouble[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSBrS( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBrS", 2, arg2, test_jbyte[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSBSBrS( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrS", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrS", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrS", 4, arg4, test_jbyte[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSSrS( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSrS", 2, arg2, test_jshort[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSSSSrS( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrS", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrS", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrS", 4, arg4, test_jshort[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSIrS( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSIrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSIrS", 2, arg2, test_jint[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSISIrS( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSISIrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSISIrS", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSISIrS", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeSISIrS", 4, arg4, test_jint[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSJrS( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJrS", 2, arg2, test_jlong[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSJSJrS( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrS", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrS", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrS", 4, arg4, test_jlong[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSFrS( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFrS", 2, arg2, test_jfloat[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSFSFrS( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrS", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrS", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrS", 4, arg4, test_jfloat[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSDrS( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDrS", 2, arg2, test_jdouble[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeSDSDrS( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrS", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrS", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrS", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrS", 4, arg4, test_jdouble[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIBrS( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBrS", 2, arg2, test_jbyte[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeIBIBrS( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBIBrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrS", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIBIBrS", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrS", 4, arg4, test_jbyte[4]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeISrS( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISrS", 2, arg2, test_jshort[2]);
	}
	return test_jshort[0];
}

jshort JNICALL Java_JniArgTests_nativeISISrS( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISISrS", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISISrS", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeISISrS", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeISISrS", 4, arg4, test_jshort[4]);
	}
	return test_jshort[0];
}





