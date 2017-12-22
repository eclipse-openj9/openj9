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

#if !defined(FASTJNI_H)
#define FASTJNI_H

#include "j9cfg.h"
#include "j9comp.h"

/* Flag constants */
#define J9_FAST_JNI_RETAIN_VM_ACCESS		1
#define J9_FAST_JNI_NOT_GC_POINT			2
#define J9_FAST_JNI_NO_NATIVE_METHOD_FRAME	4
#define J9_FAST_JNI_NO_EXCEPTION_THROW		8
#define J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN	16
#define J9_FAST_JNI_DO_NOT_WRAP_OBJECTS		32
#define J9_FAST_JNI_DO_NOT_PASS_RECEIVER	64
#define J9_FAST_JNI_DO_NOT_PASS_THREAD		128

/* Legacy constant */
#define J9_FAST_NO_NATIVE_METHOD_FRAME		J9_FAST_JNI_NO_NATIVE_METHOD_FRAME

/* Structure and macro definitions for fast native method tables */

typedef struct {
	const char *methodName;
	UDATA methodNameLength;
	const char *methodSignature;
	UDATA methodSignatureLength;
	UDATA flags;
	void * function;
} J9FastJNINativeMethodDescriptor;

#define J9_FAST_JNI_METHOD_TABLE_EXTERN(tableName) extern J9FastJNINativeMethodDescriptor FastJNINatives_##tableName[]
#define J9_FAST_JNI_METHOD_TABLE(tableName) J9FastJNINativeMethodDescriptor FastJNINatives_##tableName[] = {
#define J9_FAST_JNI_METHOD(name, sig, func, flags) { name, sizeof(name) - 1, sig, sizeof(sig) - 1, flags, (void*)func },
#define J9_FAST_JNI_METHOD_TABLE_END J9_FAST_JNI_METHOD(NULL, NULL, NULL, 0) };

typedef struct {
	const char *className;
	UDATA classNameLength;
	J9FastJNINativeMethodDescriptor *natives;
} J9FastJNINativeClassDescriptor;

#define J9_FAST_JNI_CLASS_TABLE(tableName) J9FastJNINativeClassDescriptor tableName[] = {
#define J9_FAST_JNI_CLASS(className, table) { className, sizeof(className) - 1, FastJNINatives_##table },
#define J9_FAST_JNI_CLASS_TABLE_END { NULL, 0, NULL } };
#endif
