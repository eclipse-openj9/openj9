
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

jboolean JNICALL Java_JniArgTests_nativeIZrZ(JNIEnv *p_env, jobject p_this, jint value, jbooleanArray backChannel)
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	jboolean converted = (jboolean)value;
	jboolean elements[] = {JNI_FALSE};
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests++;

	if (converted) {
		/* if C could would see this as true, set the backchannel to true */
		elements[0] = JNI_TRUE;
	}

	(*p_env)->SetBooleanArrayRegion(p_env, backChannel, 0, 1, elements);

	return value;   /* return the value as a boolean */
}

jboolean JNICALL Java_JniArgTests_nativeZIZIZIZIZIrZ(JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7, jint arg8, jint arg9, jint arg10)
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests++;

	if (arg1 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 1, arg1, test_jboolean[0]);
	}
	if (arg2 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 2, arg2, test_jint[1]);
	}
	if (arg3 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 3, arg3, test_jboolean[1]);
	}
	if (arg4 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 4, arg4, test_jint[2]);
	}
	if (arg5 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 5, arg5, test_jboolean[0]);
	}
	if (arg6 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 6, arg6, test_jint[3]);
	}
	if (arg7 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 7, arg7, test_jboolean[1]);
	}
	if (arg8 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 8, arg8, test_jint[4]);
	}
	if (arg9 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 9, arg9, test_jboolean[0]);
	}
	if (arg10 != test_jint[5]) {
		cFailure_jint(PORTLIB, "nativeZIZIZIZIZIrZ", 10, arg10, test_jint[5]);
	}

	return (JNI_TRUE == test_jboolean[0]);
}

jboolean JNICALL Java_JniArgTests_nativeIZIZIZIZIZrZ(JNIEnv *p_env, jobject p_this, jint arg1, jboolean arg2, jint arg3, jboolean arg4, jint arg5, jboolean arg6, jint arg7, jboolean arg8, jint arg9, jboolean arg10)
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests++;

	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIZIZIZIZIZrZ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jboolean[1]) {
		cFailure_jboolean(PORTLIB, "nativeIZIZIZIZIZrZ", 2, arg2, test_jboolean[1]);
	}
	if (arg3 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIZIZIZIZIZrZ", 3, arg3, test_jint[2]);
	}
	if (arg4 != test_jboolean[0]) {
		cFailure_jboolean(PORTLIB, "nativeIZIZIZIZIZrZ", 4, arg4, test_jboolean[0]);
	}
	if (arg5 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIZIZIZIZIZrZ", 5, arg5, test_jint[3]);
	}
	if (arg6 != test_jboolean[1]) {
		cFailure_jboolean(PORTLIB, "nativeIZIZIZIZIZrZ", 6, arg6, test_jboolean[1]);
	}
	if (arg7 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIZIZIZIZIZrZ", 7, arg7, test_jint[4]);
	}
	if (arg8 != test_jboolean[0]) {
		cFailure_jboolean(PORTLIB, "nativeIZIZIZIZIZrZ", 8, arg8, test_jboolean[0]);
	}
	if (arg9 != test_jint[5]) {
		cFailure_jint(PORTLIB, "nativeIZIZIZIZIZrZ", 9, arg9, test_jint[5]);
	}
	if (arg10 != test_jboolean[1]) {
		cFailure_jboolean(PORTLIB, "nativeIZIZIZIZIZrZ", 10, arg10, test_jboolean[1]);
	}

	return (JNI_FALSE == test_jboolean[0]);
}

jboolean JNICALL Java_JniArgTests_nativeZZZZZZZZZZrZ(JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7, jint arg8, jint arg9, jint arg10)
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests++;

	if (arg1 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 1, arg1, test_jboolean[1]);
	}
	if (arg2 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 2, arg2, test_jboolean[0]);
	}
	if (arg3 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 3, arg3, test_jboolean[1]);
	}
	if (arg4 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 4, arg4, test_jboolean[0]);
	}
	if (arg5 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 5, arg5, test_jboolean[1]);
	}
	if (arg6 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 6, arg6, test_jboolean[0]);
	}
	if (arg7 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 7, arg7, test_jboolean[1]);
	}
	if (arg8 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 8, arg8, test_jboolean[0]);
	}
	if (arg9 != test_jboolean[1]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 9, arg9, test_jboolean[1]);
	}
	if (arg10 != test_jboolean[0]) {
		cFailure_jint(PORTLIB, "nativeZZZZZZZZZZrZ", 10, arg10, test_jboolean[0]);
	}

	return test_jboolean[0];
}

jboolean JNICALL Java_JniArgTests_nativeIIIIIZZZZZrZ(JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jboolean arg6, jboolean arg7, jboolean arg8, jboolean arg9, jboolean arg10)
{
	J9JavaVM *javaVM = getJ9JavaVM(p_env);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	jniTests++;

	if (arg1 != test_jint[1]) {
		cFailure_jint(PORTLIB, "nativeIIIIIZZZZZrZ", 1, arg1, test_jint[1]);
	}
	if (arg2 != test_jint[2]) {
		cFailure_jint(PORTLIB, "nativeIIIIIZZZZZrZ", 2, arg2, test_jint[2]);
	}
	if (arg3 != test_jint[3]) {
		cFailure_jint(PORTLIB, "nativeIIIIIZZZZZrZ", 3, arg3, test_jint[3]);
	}
	if (arg4 != test_jint[4]) {
		cFailure_jint(PORTLIB, "nativeIIIIIZZZZZrZ", 4, arg4, test_jint[4]);
	}
	if (arg5 != test_jint[5]) {
		cFailure_jint(PORTLIB, "nativeIIIIIZZZZZrZ", 5, arg5, test_jint[5]);
	}
	if (arg6 != test_jboolean[0]) {
		cFailure_jboolean(PORTLIB, "nativeIIIIIZZZZZrZ", 6, arg6, test_jboolean[0]);
	}
	if (arg7 != test_jboolean[1]) {
		cFailure_jboolean(PORTLIB, "nativeIIIIIZZZZZrZ", 7, arg7, test_jboolean[1]);
	}
	if (arg8 != test_jboolean[0]) {
		cFailure_jboolean(PORTLIB, "nativeIIIIIZZZZZrZ", 8, arg8, test_jboolean[0]);
	}
	if (arg9 != test_jboolean[1]) {
		cFailure_jboolean(PORTLIB, "nativeIIIIIZZZZZrZ", 9, arg9, test_jboolean[1]);
	}
	if (arg10 != test_jboolean[0]) {
		cFailure_jboolean(PORTLIB, "nativeIIIIIZZZZZrZ", 10, arg10, test_jboolean[0]);
	}

	return test_jboolean[1];
}
