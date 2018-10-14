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
#include "j9comp.h"
#include "jni.h"
#include "j9.h"

/**
  * Throw java.lang.IndexOutOfBoundsException
  */
void
throwNewIndexOutOfBoundsException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/IndexOutOfBoundsException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/**
  * Throw java.lang.InternalError
  */
void
throwNewInternalError(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/InternalError");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/**
  * Throw java.lang.NullPointerException with the message provided
  */
void
throwNewNullPointerException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/NullPointerException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/**
  * Throw java.lang.IllegalArgumentException
  */
void
throwNewIllegalArgumentException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/IllegalArgumentException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/**
  * Throw java.lang.IllegalStateException
  */
void
throwNewIllegalStateException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/lang/IllegalStateException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}


/**
  * Throw java.util.zip.ZipException with the message provided
  */
void
throwNewJavaZIOException(JNIEnv *env, char *message)
{
	jclass exceptionClass = (*env)->FindClass(env, "java/util/zip/ZipException");
	if (exceptionClass == 0) {
		/* Just return if we can't load the exception class. */
		return;
	}
	(*env)->ThrowNew(env, exceptionClass, message);
}

/**
 * Throw java/lang/UnsupportedOperationException
 */
void
throwNewUnsupportedOperationException(JNIEnv *env)
{
	jclass clazz = (*env)->FindClass(env, "java/lang/UnsupportedOperationException");
	if (NULL != clazz) {
		(*env)->ThrowNew(env, clazz, NULL);
	}
}

