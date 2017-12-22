/*******************************************************************************
 * Copyright (c) 2013, 2014 IBM Corp. and others
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

/*
 * Created 1 February 2013 by braddill@ca.ibm.com
 *
 * This file contains the JNI code used by j9vm.test.classloader.LazyClassLoaderInitTest in j9vm_test.
 */

#include "j9.h"
#include "jni.h"

#include <jvmti.h>
#include <assert.h>
#include <stdlib.h>

jobject JNICALL JVM_FindClassFromClassLoader(JNIEnv* env, char* className, jboolean init, jobject classLoader, jboolean throwError);

/**
 * Define a class from within a JNI call.
 *
 * @param classname
 *            the name of the class to define.
 * @param bytecode
 *            the bytecode to use for the class.
 * @param loader
 *            the classloader to use.
 * @return the defined Class.
 */
jclass JNICALL
Java_j9vm_test_classloader_LazyClassLoaderInitTest_jniDefineClass(JNIEnv *env, jobject caller, jstring classname, jbyteArray bytecode, jobject loader)
{
	jclass clazz = NULL; /* The new class defined. */
	const char *name = NULL; /* classname in UTF-8 */

	/* Convert classname to UTF string. */
	name = (*env)->GetStringUTFChars(env, classname, NULL);
	if (NULL != name) {
		/* Convert Java array to C buffer for use in defineClass call. */
		jbyte *buf    = (*env)->GetByteArrayElements(env, bytecode, NULL);
		if (NULL != buf) {
			jsize  bufLen = (*env)->GetArrayLength(env, bytecode);
			/* Define the new class. */
			clazz = (*env)->DefineClass(env, name, loader, buf, bufLen);

			/* Release any allocated memory and abandon any change to it. */
			(*env)->ReleaseByteArrayElements(env, bytecode, buf, JNI_ABORT);
		} else {
			jclass errorClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
			if (NULL != errorClass) {
				(*env)->ThrowNew(env, errorClass, "couldn't GetByteArrayElements");
			}
			return NULL;
		}
		(*env)->ReleaseStringUTFChars(env, classname, name);
	} else {
		jclass errorClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		if (NULL != errorClass) {
			(*env)->ThrowNew(env, errorClass, "couldn't GetStringUTFChars");
		}
		return NULL;
	}

	return clazz;	/* Return the new class to the Java caller. */
}

/**
 * Find a Class using the internal function
 * JVM_FindClassFromClassLoader_Impl.
 *
 * @param classname
 *            the class name to find.
 * @param classloader
 *            the class loader to use.
 * @return the class found.
 */
jobject JNICALL
Java_j9vm_test_classloader_LazyClassLoaderInitTest_jvmFindClass(JNIEnv *env, jobject caller, jstring classname, jobject loader)
{
	const char *nameUTF = NULL; /* const classname in UTF */
	jobject obj = NULL;
	jboolean init = JNI_TRUE;
	jboolean throwError = JNI_TRUE;

	/* Convert classname to const UTF string. */
	nameUTF = (*env)->GetStringUTFChars(env, classname, NULL);
	if (NULL != nameUTF) {
		/**
		 * This function makes sure that call to "internalFindClassFromClassLoader" is gpProtected
		 *
		 * @param env
		 * @param name         null-terminated class name string.
		 * @param init         initialize the class when set
		 * @param loader       classloader of the class
		 * @param throwError   set to true in order to throw errors
		 * @return Assumed to be a jclass.
		 */
		obj = JVM_FindClassFromClassLoader(env, (char*)nameUTF, init, loader, throwError);

		/* Release any allocated memory and abandon any change to it. */
		(*env)->ReleaseStringUTFChars(env, classname, nameUTF);
	} else {
		jclass errorClass = (*env)->FindClass(env, "java/lang/OutOfMemoryError");
		if (NULL != errorClass) {
			(*env)->ThrowNew(env, errorClass, "couldn't GetStringUTFChars");
		}
		return NULL;
	}

	return obj;
}

/**
 * Use JVMTI to test and verify an empty class loader.
 *
 * @return number of classes loaded by classloader.
 */
jint JNICALL
Java_j9vm_test_classloader_LazyClassLoaderInitTest_jvmtiCountLoadedClasses(JNIEnv *env, jobject caller, jobject loader)
{
	jint numClasses = 0;
	jclass *classes_ptr = NULL;
	JavaVM *jvm = NULL;
	jvmtiEnv *jvmti = NULL;

	/* Set up a JVMTI env. */
	(*env)->GetJavaVM(env, &jvm);
	(*jvm)->GetEnv(jvm, (void **)&jvmti, JVMTI_VERSION_1_0);
	if (NULL == jvmti) {
		jclass errorClass = (*env)->FindClass(env, "java/lang/Error");
		if (NULL != errorClass) {
			(*env)->ThrowNew(env, errorClass, "couldn't get a JVMTI env");
		}
		return -1;
	}

	/* Get the classes loaded by loader. */
	(*jvmti)->GetClassLoaderClasses(jvmti, loader, &numClasses, &classes_ptr);

	/* Free the array, all we need is the count. */
	(*jvmti)->Deallocate(jvmti, (unsigned char*) classes_ptr);

	/* Clean up JVMTI. */
	(*jvmti)->DisposeEnvironment(jvmti);

	return numClasses;
}

