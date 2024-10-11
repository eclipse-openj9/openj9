/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "jni.h"

extern "C" {

jlong JNICALL
Java_jdk_jfr_internal_JVM_nanosNow(JNIEnv *env, jclass clazz)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_hostTotalSwapMemory(JNIEnv *env, jclass clazz)
{
	// TODO: implementation
	return 0;
}

jlong JNICALL
Java_jdk_jfr_internal_JVM_registerStackFilter(JNIEnv *env, jclass clazz, jobjectArray classes, jobjectArray methods)
{
	// TODO: implementation
	return 0;
}

void JNICALL
Java_jdk_jfr_internal_JVM_unregisterStackFilter(JNIEnv *env, jclass clazz, jlong stackFilterId)
{
	// TODO: implementation
}

void JNICALL
Java_jdk_jfr_internal_JVM_setMiscellaneous(JNIEnv *env, jclass clazz, jlong eventTypeId, jlong value)
{
	// TODO: implementation
}

} /* extern "C" */
