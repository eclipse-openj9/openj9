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
#include "jvminit.h"
#include "verbose_api.h"

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_ClassLoadingMXBeanImpl_isVerboseImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	return ( (javaVM->verboseLevel & VERBOSE_CLASS) == VERBOSE_CLASS );
}

jint JNICALL
Java_com_ibm_java_lang_management_internal_ClassLoadingMXBeanImpl_getLoadedClassCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	jint result;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read( mgmt->managementDataLock );

	result = (jint)( mgmt->totalClassLoads - mgmt->totalClassUnloads );

	omrthread_rwmutex_exit_read( mgmt->managementDataLock );

	return result;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_ClassLoadingMXBeanImpl_getTotalLoadedClassCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	jlong result;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read( mgmt->managementDataLock );

	result = (jlong)mgmt->totalClassLoads;

	omrthread_rwmutex_exit_read( mgmt->managementDataLock );

	return result;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_ClassLoadingMXBeanImpl_getUnloadedClassCountImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	jlong result;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	omrthread_rwmutex_enter_read( mgmt->managementDataLock );

	result = (jlong)mgmt->totalClassUnloads;

	omrthread_rwmutex_exit_read( mgmt->managementDataLock );

	return result;
}

void JNICALL
Java_com_ibm_java_lang_management_internal_ClassLoadingMXBeanImpl_setVerboseImpl(JNIEnv *env, jobject beanInstance, jboolean value)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9VerboseSettings verboseOptions;

	memset(&verboseOptions, 0, sizeof(J9VerboseSettings));
	if( javaVM->setVerboseState != NULL ) {
		verboseOptions.vclass = value? VERBOSE_SETTINGS_SET: VERBOSE_SETTINGS_CLEAR;
		javaVM->setVerboseState( javaVM, &verboseOptions, NULL );
	}
}
