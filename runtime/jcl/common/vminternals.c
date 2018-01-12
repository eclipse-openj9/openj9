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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include "jni.h"
#include "j9.h"
#include "jclglob.h"
#include "ut_j9jcl.h"

jobject JNICALL
Java_java_lang_J9VMInternals_newInstance(JNIEnv *env, jclass clazz, jobject instantiationClass, jobject constructorClass)
{
	jmethodID mid = (* env) -> GetMethodID(env, constructorClass, "<init>", "()V");

	if (mid == 0) {
		/* Cant newInstance,No empty constructor... */
		return (jobject)0;
	} else {
		jobject obj;
		/* Instantiate an object of a given class and construct it using the constructor from the other class.
		 *
		 * Do not use NewObject to avoid -Xcheck:jni reporting an error (constructor class != allocation class).
		*/
		obj = (* env) -> AllocObject(env, instantiationClass);
		if (obj != NULL) {
			(* env) -> CallVoidMethod(env, obj, mid);
		}
		return obj;
	}
}

void JNICALL
Java_java_lang_J9VMInternals_dumpString(JNIEnv * env, jclass clazz, jstring str)
{
	PORT_ACCESS_FROM_ENV(env);

	if (str == NULL) {
		j9tty_printf(PORTLIB, "null");
	} else {
		/* Note: GetStringUTFChars nul-terminates the resulting chars, even though the spec does not say so */
		const char * utfChars = (const char *) (*env)->GetStringUTFChars(env, str, NULL);

		if (utfChars != NULL) {
			j9tty_printf(PORTLIB, "%s", utfChars);
			(*env)->ReleaseStringUTFChars(env, str, utfChars);
		}
	}
}


