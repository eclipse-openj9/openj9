/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

#include "jni.h"
#include "j2sever.h"
#include "j9.h"
#include "j9port.h"

jboolean JNICALL Java_java_security_AccessController_initializeInternal(JNIEnv *env, jclass thisClz)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	jclass accessControllerClass;
	jmethodID mid;

	accessControllerClass = (*env)->FindClass(env, "java/security/AccessController");
	if(accessControllerClass == NULL) goto fail;

	mid = (*env)->GetStaticMethodID(env, accessControllerClass, "doPrivileged", "(Ljava/security/PrivilegedAction;)Ljava/lang/Object;");
	if(mid == NULL) goto fail;
	javaVM->doPrivilegedMethodID1 = (UDATA) mid;

	mid = (*env)->GetStaticMethodID(env, accessControllerClass, "doPrivileged", "(Ljava/security/PrivilegedExceptionAction;)Ljava/lang/Object;");
	if(mid == NULL) goto fail;
	javaVM->doPrivilegedMethodID2 = (UDATA) mid;

	mid = (*env)->GetStaticMethodID(env, accessControllerClass, "doPrivileged", "(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;");
	if(mid == NULL) goto fail;
	javaVM->doPrivilegedWithContextMethodID1 = (UDATA) mid;

	mid = (*env)->GetStaticMethodID(env, accessControllerClass, "doPrivileged", "(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;)Ljava/lang/Object;");
	if(mid == NULL) goto fail;
	javaVM->doPrivilegedWithContextMethodID2 = (UDATA) mid;

	mid = (*env)->GetStaticMethodID(env, accessControllerClass, "doPrivileged", "(Ljava/security/PrivilegedAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;");
	if (NULL == mid) goto fail;
	javaVM->doPrivilegedWithContextPermissionMethodID1 = (UDATA) mid;

	mid = (*env)->GetStaticMethodID(env, accessControllerClass, "doPrivileged", "(Ljava/security/PrivilegedExceptionAction;Ljava/security/AccessControlContext;[Ljava/security/Permission;)Ljava/lang/Object;");
	if (NULL == mid) goto fail;
	javaVM->doPrivilegedWithContextPermissionMethodID2 = (UDATA) mid;

	return JNI_TRUE;

fail:
	return JNI_FALSE;
}
