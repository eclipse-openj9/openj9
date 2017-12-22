
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



jint JNICALL Java_JniArgTests_nativeFFrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFrI", 2, arg2, test_jfloat[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFFFFrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrI", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrI", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeFFFFrI", 4, arg4, test_jfloat[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFDrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDrI", 2, arg2, test_jdouble[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeFDFDrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jfloat[1]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrI", 1, arg1, test_jfloat[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrI", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jfloat[3]) {
		cFailure_jfloat(PORTLIB, "nativeFDFDrI", 3, arg3, test_jfloat[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeFDFDrI", 4, arg4, test_jdouble[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDBrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBrI", 2, arg2, test_jbyte[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDBDBrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrI", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDBDBrI", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeDBDBrI", 4, arg4, test_jbyte[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDSrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSrI", 2, arg2, test_jshort[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDSDSrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrI", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDSDSrI", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeDSDSrI", 4, arg4, test_jshort[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDIrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIrI", 2, arg2, test_jint[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDIDIrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeDIDIrI", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDIDIrI", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeDIDIrI", 4, arg4, test_jint[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDJrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJrI", 2, arg2, test_jlong[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDJDJrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrI", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDJDJrI", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeDJDJrI", 4, arg4, test_jlong[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDFrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFrI", 2, arg2, test_jfloat[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDFDFrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrI", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDFDFrI", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeDFDFrI", 4, arg4, test_jfloat[4]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDDrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDrI", 2, arg2, test_jdouble[2]);
	}
	return test_jint[0];
}

jint JNICALL Java_JniArgTests_nativeDDDDrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jdouble[1]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrI", 1, arg1, test_jdouble[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrI", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jdouble[3]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrI", 3, arg3, test_jdouble[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeDDDDrI", 4, arg4, test_jdouble[4]);
	}
	return test_jint[0];
}

jlong JNICALL Java_JniArgTests_nativeBBrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBrJ", 2, arg2, test_jbyte[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBBBBrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrJ", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrJ", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeBBBBrJ", 4, arg4, test_jbyte[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBSrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSrJ", 2, arg2, test_jshort[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBSBSrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrJ", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBSBSrJ", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeBSBSrJ", 4, arg4, test_jshort[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBIrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIrJ", 2, arg2, test_jint[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBIBIrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeBIBIrJ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBIBIrJ", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeBIBIrJ", 4, arg4, test_jint[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBJrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJrJ", 2, arg2, test_jlong[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBJBJrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrJ", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBJBJrJ", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeBJBJrJ", 4, arg4, test_jlong[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBFrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFrJ", 2, arg2, test_jfloat[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBFBFrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrJ", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBFBFrJ", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeBFBFrJ", 4, arg4, test_jfloat[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBDrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDrJ", 2, arg2, test_jdouble[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeBDBDrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jbyte[1]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrJ", 1, arg1, test_jbyte[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrJ", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jbyte[3]) {
		cFailure_jbyte(PORTLIB, "nativeBDBDrJ", 3, arg3, test_jbyte[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeBDBDrJ", 4, arg4, test_jdouble[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSBrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBrJ", 2, arg2, test_jbyte[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSBSBrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrJ", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSBSBrJ", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeSBSBrJ", 4, arg4, test_jbyte[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSSrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSrJ", 2, arg2, test_jshort[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSSSSrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrJ", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrJ", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeSSSSrJ", 4, arg4, test_jshort[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSIrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSIrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSIrJ", 2, arg2, test_jint[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSISIrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSISIrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeSISIrJ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSISIrJ", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeSISIrJ", 4, arg4, test_jint[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSJrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJrJ", 2, arg2, test_jlong[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSJSJrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrJ", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSJSJrJ", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeSJSJrJ", 4, arg4, test_jlong[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSFrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFrJ", 2, arg2, test_jfloat[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSFSFrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrJ", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSFSFrJ", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeSFSFrJ", 4, arg4, test_jfloat[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSDrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDrJ", 2, arg2, test_jdouble[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeSDSDrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jshort[1]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrJ", 1, arg1, test_jshort[1]);
	}
	if (arg2 != test_jdouble[2]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrJ", 2, arg2, test_jdouble[2]);
	}
	if (arg3 != test_jshort[3]) {
		cFailure_jshort(PORTLIB, "nativeSDSDrJ", 3, arg3, test_jshort[3]);
	}
	if (arg4 != test_jdouble[4]) {
		cFailure_jdouble(PORTLIB, "nativeSDSDrJ", 4, arg4, test_jdouble[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIBrJ( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBrJ", 2, arg2, test_jbyte[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIBIBrJ( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIBIBrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jbyte[2]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrJ", 2, arg2, test_jbyte[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIBIBrJ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jbyte[4]) {
		cFailure_jbyte(PORTLIB, "nativeIBIBrJ", 4, arg4, test_jbyte[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeISrJ( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISrJ", 2, arg2, test_jshort[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeISISrJ( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeISISrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jshort[2]) {
		cFailure_jshort(PORTLIB, "nativeISISrJ", 2, arg2, test_jshort[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeISISrJ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jshort[4]) {
		cFailure_jshort(PORTLIB, "nativeISISrJ", 4, arg4, test_jshort[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIIrJ( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIrJ", 2, arg2, test_jint[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIIIIrJ( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIIIrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIIIrJ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIIIIrJ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIIIIrJ", 4, arg4, test_jint[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIJrJ( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJrJ", 2, arg2, test_jlong[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIJIJrJ( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIJIJrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jlong[2]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrJ", 2, arg2, test_jlong[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIJIJrJ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jlong[4]) {
		cFailure_jlong(PORTLIB, "nativeIJIJrJ", 4, arg4, test_jlong[4]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIFrJ( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFrJ", 2, arg2, test_jfloat[2]);
	}
	return test_jlong[0];
}

jlong JNICALL Java_JniArgTests_nativeIFIFrJ( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 )
{	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests ++;


	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIFIFrJ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jfloat[2]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrJ", 2, arg2, test_jfloat[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIFIFrJ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jfloat[4]) {
		cFailure_jfloat(PORTLIB, "nativeIFIFrJ", 4, arg4, test_jfloat[4]);
	}
	return test_jlong[0];
}





