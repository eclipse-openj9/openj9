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
#include "jcl_internal.h"

jlong JNICALL
Java_com_ibm_java_lang_management_internal_CompilationMXBeanImpl_getTotalCompilationTimeImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	jlong result;
	J9JavaLangManagementData *mgmt = javaVM->managementData;
	PORT_ACCESS_FROM_JAVAVM( javaVM );

	omrthread_rwmutex_enter_read( mgmt->managementDataLock );

	result = (jlong)mgmt->totalCompilationTime;
	if( mgmt->threadsCompiling > 0 ) {
		result += checkedTimeInterval((U_64)j9time_nano_time(), (U_64)mgmt->lastCompilationStart) * mgmt->threadsCompiling;
	}

	omrthread_rwmutex_exit_read( mgmt->managementDataLock );

	return result / J9PORT_TIME_NS_PER_MS;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_CompilationMXBeanImpl_isCompilationTimeMonitoringSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	return JNI_TRUE;
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_CompilationMXBeanImpl_isJITEnabled(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

#if defined (J9VM_INTERP_NATIVE_SUPPORT)
	if( javaVM->jitConfig != NULL ) {
		return JNI_TRUE;
	}
#endif

	return JNI_FALSE;
}
