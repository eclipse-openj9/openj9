/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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

/**
 * @file static_libraries.c
 * @brief Provides definitions for JNI routines statically.
 */
#include "testjep178.h"

/**
 * @brief Function indicates to the runtime that the library testlibA has been
 * linked into the executable.
 */
JNIEXPORT jint JNICALL 
JNI_OnLoad_testlibA(JavaVM *vm, void *reserved)
{
	fprintf(stdout, "[MSG] Reached OnLoad: JNI_OnLoad_testlibA [statically]\n");
	fflush(stdout);
	return JNI_VERSION_1_8;
}

/**
 * @brief Function indicates an alternative to the traditional unload routine to
 * the runtime specifically targeting the library testlibA.
 */
JNIEXPORT void JNICALL 
JNI_OnUnload_testlibA(JavaVM *vm, void *reserved)
{
	fprintf(stdout, "[MSG] Reached OnUnload: JNI_OnUnload_testlibA [statically]\n");
	fflush(stdout);
	/* Nothing much to cleanup here. */
	return;
}

/**
 * @brief Function indicates to the runtime that the library testlibB has been
 * linked into the executable.
 */
JNIEXPORT jint JNICALL 
JNI_OnLoad_testlibB(JavaVM *vm, void *reserved)
{
	fprintf(stdout, "[MSG] Reached OnLoad: JNI_OnLoad_testlibB [statically]\n");
	fflush(stdout);
	return JNI_VERSION_1_8;
}

/**
 * @brief Function indicates an alternative to the traditional unload routine to
 * the runtime specifically targeting the library testlibB.
 */
JNIEXPORT void JNICALL 
JNI_OnUnload_testlibB(JavaVM *vm, void *reserved)
{
	fprintf(stdout, "[MSG] Reached OnUnload: JNI_OnUnload_testlibB [statically]\n");
	fflush(stdout);
	/* Nothing much to cleanup here. */
	return;
}

/**
 * @brief Provide an implementation for the class StaticLinking's native instance 
 * method fooImpl.
 * Package:	  com.ibm.j9.tests.jeptests
 * Class:     StaticLinking
 * Method:    fooImpl
 * @param[in] env The JNI env.
 * @param[in] instance The this pointer.
 */
JNIEXPORT void JNICALL 
Java_com_ibm_j9_tests_jeptests_StaticLinking_fooImpl(JNIEnv *env, jobject this)
{
	fprintf(stdout, "[MSG] Reached native fooImpl() [statically]\n");
	fflush(stdout);
}

/**
 * @brief Provide an implementation for the class StaticLinking's native static 
 * method barImpl.
 * Package:	  com.ibm.j9.tests.jeptests
 * Class:     StaticLinking
 * Method:    barImpl
 * @param[in] env The JNI env.
 * @param[in] Class The class pointer.
 */
JNIEXPORT void JNICALL 
Java_com_ibm_j9_tests_jeptests_StaticLinking_barImpl(JNIEnv *env, jclass Class)
{
	fprintf(stdout, "[MSG] Reached native barImpl() [statically]\n");
	fflush(stdout);
}
