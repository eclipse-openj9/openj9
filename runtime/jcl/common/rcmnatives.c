/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "jvminit.h"

#include "vmaccess.h"

#include "j9rcm_resource_types.h"
#include "rcm_vmi_api.h"

/**
 * Request token from HTB
 *
 * @param env
 * @param runnable The CPUThrottlingRunnable object.
 * @param tokenNumber The number of tokens that need to be requested from HTB.
 */
jlong JNICALL
Java_javax_rcm_CPUThrottlingRunnable_requestToken(JNIEnv *env, jobject runnable, jlong tokenNumber)
{
	const static int resType = CPU;
	J9VMThread *vmThread = (J9VMThread *)env;
	JavaVM *vm = (JavaVM *)vmThread->javaVM;
	J9RCM_VMI *vmi = NULL;
	void *penv;

	(*vm)->GetEnv(vm, &penv, J9RCM_VMI_VERSION_1_2);
	vmi = (J9RCM_VMI*)(penv);
	if (vmi != NULL) {
		if (vmi->J9RCM_IsEnabled(vmi)) {
			J9_RCM_VRC_HANDLE vrcHandle = vmi->J9RCM_GetVirtualResourceContainerHandle(vmi, resType);
			if (NULL != vrcHandle) {
				U_64 request_interval = 0;
				U_64 allowed = 0;
				const static BOOLEAN allowPartial = TRUE;

				allowed = vmi->J9RCM_HTB_RequestToken(vmi, vrcHandle, &request_interval, tokenNumber, allowPartial);
				if(0 != allowed){
					vmi->J9RCM_ACC_RecordConsumption(vmi, vrcHandle, allowed);
				}
				vmi->J9RCM_ReleaseVirtualResourceContainerHandle(vmi, vrcHandle);
				return (jlong)allowed;
			}
		}
	}

	return tokenNumber;
}

/**
 * Get height of TokenBucket.
 *
 * @param env
 * @param clazz The CPUThrottlingRunnable class
 * @param resourceHandle  Handle of the resource whose TokenBucket height is queried.
 */
jlong JNICALL
Java_javax_rcm_CPUThrottlingRunnable_getTokenBucketLimit(JNIEnv *env, jclass clazz, jlong resourceHandle)
{
	const static int resType = CPU;
	J9VMThread *vmThread = (J9VMThread *)env;
	JavaVM *vm = (JavaVM *)vmThread->javaVM;
	J9RCM_VMI *vmi = NULL;
	void *penv;

	(*vm)->GetEnv(vm, &penv, J9RCM_VMI_VERSION_1_2);
	vmi = (J9RCM_VMI*)(penv);
	if (vmi != NULL) {
		if (vmi->J9RCM_IsEnabled(vmi)) {
			U_64 tokenBucketLimit = vmi->J9RCM_HTB_RES_GetCR(vmi, resourceHandle);
			return tokenBucketLimit;
		}
	}
	return -1L;
}

/**
 * Get refill interval of TokenBucket.
 *
 * @param env
 * @param clazz The CPUThrottlingRunnable class
 * @param resourceHandle  Handle of the resource whose TokenBucket refill interval is queried.
 */
jlong JNICALL
Java_javax_rcm_CPUThrottlingRunnable_getTokenBucketInterval(JNIEnv *env, jclass clazz, jlong resourceHandle)
{
	const static int resType = CPU;
	J9VMThread *vmThread = (J9VMThread *)env;
	JavaVM *vm = (JavaVM *)vmThread->javaVM;
	J9RCM_VMI *vmi = NULL;
	void *penv;

	(*vm)->GetEnv(vm, &penv, J9RCM_VMI_VERSION_1_2);
	vmi = (J9RCM_VMI*)(penv);
	if (vmi != NULL) {
		if (vmi->J9RCM_IsEnabled(vmi)) {
			UDATA tokenBucketInterval = vmi->J9RCM_HTB_RES_GetRate(vmi, resourceHandle);
			return (jlong)tokenBucketInterval;
		}
	}
	return -1L;
}

