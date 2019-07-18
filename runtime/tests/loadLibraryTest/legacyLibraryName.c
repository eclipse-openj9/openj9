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

/**
 * @file legacyLibraryName.c
 * @brief File is to be compiled into a shared library (legacyName).  All the symbols should
 * be exported and visible for external invocations.
 */

#include "jni.h"

/**
 * @brief Function indicates to the runtime that the library legacyLibraryName has been
 * linked into the executable.
 */
jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    return JNI_VERSION_1_8;
}

/**
 * @brief Function indicates an alternative to the traditional unload routine to
 * the runtime specifically targeting the library legaclegacyLibraryNameyName.
 */
void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    return;
}

/**
 * @brief Provide an implementation for the class TestLoadLegacyLibrary's native method fooImpl.
 * Package:     org.openj9.test.loadLibrary
 * Class:       TestLoadLegacyLibrary
 * Method:      fooImpl
 * @param[in]   env The JNI env.
 * @param[in]   instance The this pointer.
 * @return      Always return value "true"
 */
jboolean JNICALL
Java_j9vm_test_loadLibrary_TestLoadLegacyLibrary_fooImpl(JNIEnv *env, jobject this)
{
    return JNI_TRUE;
}
