/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#include "jclprots.h"

jint JNICALL Java_java_lang_String_getSeed(JNIEnv *env, jclass clazz)
{
	U_64 salt = 0;
	U_32 seed = 0;
	jarray array = NULL;

	PORT_ACCESS_FROM_ENV(env);

	array = (*env)->NewBooleanArray(env, 1);
	salt = j9time_hires_clock();
	salt ^= (U_64)(UDATA)(((J9VMThread*) env)->javaVM);
	salt ^= (U_64)(UDATA)array;
	seed = (U_32) (salt & (((U_64)1L << 32) - 1));
	seed ^= (salt >> 32);
	return (jint)seed;
}
