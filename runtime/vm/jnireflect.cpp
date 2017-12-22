/*******************************************************************************
 * Copyright (c) 2012, 2014 IBM Corp. and others
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

#include "jnireflect.h"

#include "j9.h"
#include "rommeth.h"
#include "util_api.h"

#include "VMAccess.hpp"

void * JNICALL
toReflectedField(JNIEnv *env, jclass clazz, void *fieldID, jboolean isStatic)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	return (void *)vmThread->javaVM->reflectFunctions.idToReflectField(vmThread, (jfieldID)fieldID);
}

void * JNICALL
toReflectedMethod(JNIEnv *env, jclass clazz, void *methodID, jboolean isStatic)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	return (void *)vmThread->javaVM->reflectFunctions.idToReflectMethod(vmThread, (jmethodID)methodID);
}

jfieldID JNICALL
fromReflectedField(JNIEnv *env, jobject field)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	jfieldID result = (jfieldID)currentThread->javaVM->reflectFunctions.reflectFieldToID(currentThread, field);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return result;
}

jmethodID JNICALL
fromReflectedMethod(JNIEnv *env, jobject method)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	jmethodID result = (jmethodID)currentThread->javaVM->reflectFunctions.reflectMethodToID(currentThread, method);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return result;
}
