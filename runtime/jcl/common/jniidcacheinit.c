/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "jclglob.h"
#include <string.h>
#include <stdlib.h>
#include "jcl.h"
#include "j2sever.h"
#include "j9lib.h"
#include "rommeth.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jclglob.h"
#include "jniidcacheinit.h"
#include "j9.h"
#include "jclprots.h"
#if defined(J9ZOS390)
#include "atoe.h"
#include <_Ccsid.h>
#endif

#include "ut_j9jcl.h"

/**
 * Initializes java/lang/String class ID in JniIDCache struct 
 *
 * @param[in] env The JNI env
 *
 * @return TRUE or FALSE based on whether successfully initializing IDs or not
 */
BOOLEAN 
initializeJavaLangStringIDCache(JNIEnv* env) 
{
	jstring jStringClass;
	jobject globalStringRef;
	BOOLEAN isGlobalRefRedundant=FALSE;
	BOOLEAN isClassIDAlreadyCached=TRUE;
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	
	/* Initializing java/lang/String class ID */
	/* Check if java/lang/String class ID hasn't been cached yet */
	omrthread_monitor_enter(javaVM->jclCacheMutex);

	if (JCL_CACHE_GET(env,CLS_java_lang_String) == NULL) {
		isClassIDAlreadyCached=FALSE;
	}

	omrthread_monitor_exit(javaVM->jclCacheMutex);
		
	if (!isClassIDAlreadyCached) {	

		/* Load java/lang/String class */ 
		jStringClass = (*env)->FindClass(env, "java/lang/String");
		if ((*env)->ExceptionCheck(env)) {
			return FALSE;
		}
	
		globalStringRef = (*env)->NewGlobalRef(env, jStringClass);
		if (globalStringRef == NULL) {
			/* out of memory exception thrown */
			javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
			return FALSE; 
		}
		
		/* Allow the local reference jStringClass to be garbage collected immediately */ 
		(*env)->DeleteLocalRef(env,jStringClass);
		
	
		omrthread_monitor_enter(javaVM->jclCacheMutex);

		if (JCL_CACHE_GET(env,CLS_java_lang_String) == NULL) {
			/* Store java/lang/String class ID in JniIDCache struct */
			JCL_CACHE_SET(env, CLS_java_lang_String, globalStringRef);
		}
		else {
			isGlobalRefRedundant=TRUE;
		}

		omrthread_monitor_exit(javaVM->jclCacheMutex);
	
		if (isGlobalRefRedundant == TRUE) {
			(*env)->DeleteGlobalRef(env,globalStringRef);
		}
	}
	return TRUE;
}


/**
 * Initializes sun/reflect/ConstantPool (Java 8) or jdk/internal/reflect/ConstantPool (Java 11+) class ID 
 * and constantPoolOop field ID in JniIDCache struct 
 *
 * @param[in] env The JNI env
 *
 * @return TRUE or FALSE based on whether successfully initializing IDs or not
 */
BOOLEAN 
initializeSunReflectConstantPoolIDCache(JNIEnv* env) 
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	BOOLEAN isClassIDAlreadyCached = TRUE;
	BOOLEAN result = TRUE;
	
	omrthread_monitor_enter(javaVM->jclCacheMutex);
	if (NULL == JCL_CACHE_GET(env, CLS_sun_reflect_ConstantPool)) {
		isClassIDAlreadyCached = FALSE;
	}
	omrthread_monitor_exit(javaVM->jclCacheMutex);
		
	if (!isClassIDAlreadyCached) {	
		jclass sunReflectConstantPool = NULL;

		/* Load sun/reflect/ConstantPool (Java 8) or jdk/internal/reflect/ConstantPool (Java 11+) */
		if (J2SE_VERSION(javaVM) >= J2SE_V11) {
			sunReflectConstantPool = (*env)->FindClass(env, "jdk/internal/reflect/ConstantPool");
		} else {
			sunReflectConstantPool = (*env)->FindClass(env, "sun/reflect/ConstantPool");
		}
		if (NULL == sunReflectConstantPool) {
			/* exception thrown */
			result = FALSE;
		} else {
			jobject globalClassRef = (*env)->NewGlobalRef(env, sunReflectConstantPool);
			/* Allow the local reference sunReflectConstantPool to be garbage collected immediately */
			(*env)->DeleteLocalRef(env, sunReflectConstantPool);
			if (NULL == globalClassRef) {
				/* out of memory exception thrown */
				javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
				result = FALSE;
			} else {
				/* Initializing constantPoolOop field ID */
				jfieldID fieldID = (*env)->GetFieldID(env, globalClassRef, "constantPoolOop", "Ljava/lang/Object;");
				if (NULL == fieldID) {
					result = FALSE;
				} else {
					BOOLEAN isGlobalRefRedundant = FALSE;
			
					omrthread_monitor_enter(javaVM->jclCacheMutex);
					if (NULL == JCL_CACHE_GET(env,CLS_sun_reflect_ConstantPool)) {
						/* Set FID_sun_reflect_ConstantPool_constantPoolOop first and CLS_sun_reflect_ConstantPool afterwards,
						 * read CLS_sun_reflect_ConstantPool first in java_lang_Access.c:Java_java_lang_Access_getConstantPool(),
						 * if NULL is returned, initializeSunReflectConstantPoolIDCache() is invoked to 
						 * set FID_sun_reflect_ConstantPool_constantPoolOop, next read it and de-reference.
						 * Otherwise, a NULL FID_sun_reflect_ConstantPool_constantPoolOop could be returned and cause gpf later.
						 */
						JCL_CACHE_SET(env, FID_sun_reflect_ConstantPool_constantPoolOop, fieldID);
						issueWriteBarrier();
						JCL_CACHE_SET(env, CLS_sun_reflect_ConstantPool, globalClassRef);
					} else {
						isGlobalRefRedundant = TRUE;
					}
					omrthread_monitor_exit(javaVM->jclCacheMutex);
				
					if (isGlobalRefRedundant) {
						(*env)->DeleteGlobalRef(env, globalClassRef);
					}
				}
			}
		}
	}
	return result;
}
