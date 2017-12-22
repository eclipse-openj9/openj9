/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef jnicsup_h
#define jnicsup_h

#include "j9.h"
#include "jni.h"
#include "j9comp.h"
#include "j9protos.h"
#include <string.h>
#include <stdlib.h>
#include "vm_internal.h"

typedef struct J9RedirectedCallInArgs {
	JNIEnv *env;
	jobject receiver;
	jclass clazz;
	jmethodID methodID;
	void *args;
} J9RedirectedCallInArgs;

typedef struct J9RedirectedToReflectedArgs {
	void *(JNICALL * func) (JNIEnv * env, jclass clazz, void *id, jboolean isStatic);
	JNIEnv *env;
	jclass clazz;
	void *id;
	jboolean isStatic;
} J9RedirectedToReflectedArgs;

typedef struct J9RedirectedInitializeArgs {
	J9VMThread* env;
	J9Class* clazz;
} J9RedirectedInitializeArgs;

typedef struct J9RedirectedSetCurrentExceptionArgs {
	J9VMThread* env;
	UDATA exceptionNumber;
	UDATA* detailMessage;
} J9RedirectedSetCurrentExceptionArgs;

typedef struct J9RedirectedSetCurrentExceptionNLSArgs {
	J9VMThread* env;
	UDATA exceptionNumber;
	U_32 moduleName;
	U_32 messageNumber;
} J9RedirectedSetCurrentExceptionNLSArgs;

typedef struct J9RedirectedFindClassArgs {
	JNIEnv *env;
	const char *name;
} J9RedirectedFindClassArgs;

#include "vm_api.h"

#endif     /* jnicsup_h */
