/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
#ifndef JNIREFLECT_H_
#define JNIREFLECT_H_

#include "j9cfg.h"
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a j.l.reflect.Field from a jfieldID.
 *
 * @param[in] env The JNI context.
 * @param[in] clazz The class containing the field.
 * @param[in] fieldID A jfieldID.
 * @param[in] isStatic JNI_TRUE if the field is static, JNI_FALSE otherwise.
 *
 * @return A j.l.reflect.Field object, or NULL on failure.
 *
 * @throws OutOfMemoryError
 */
void * JNICALL toReflectedField(JNIEnv *env, jclass clazz, void *fieldID, jboolean isStatic);

/**
 * Create a j.l.reflect.Method from a jmethodID.
 *
 * @param[in] env The JNI context.
 * @param[in] clazz The class containing the method.
 * @param[in] methodID a jmethodID.
 * @param[in] isStatic JNI_TRUE if the method is static, JNI_FALSE otherwise.
 *
 * @return a j.l.reflect.Method, or NULL on failure.
 *
 * @throws OutOfMemoryError
 */
void * JNICALL toReflectedMethod(JNIEnv *env, jclass clazz, void *methodID, jboolean isStatic);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* JNIREFLECT_H_ */
