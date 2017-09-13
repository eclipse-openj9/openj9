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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9.h"
#include "vmi.h"
#include "clear_vmi_internal.h"

/* Non-table functions */
/**
 * Extract the VM Interface from a JNI JavaVM
 *
 * @param[in] vm  The JavaVM to query
 *
 * @return a VMInterface pointer
 */
VMInterface* JNICALL 
VMI_GetVMIFromJavaVM(JavaVM* vm)
{
	VMInterface* vmInterface;

	jint rc = (*vm)->GetEnv(vm, (void**)&vmInterface, HARMONY_VMI_VERSION_2_0);
	if (0 != rc) {
		return NULL;
	}

	return vmInterface;
}	

/**
 * Extract the VM Interface from a JNIEnv
 *
 * @param[in] vm  The JNIEnv to query
 *
 * @return a VMInterface pointer
 */
VMInterface* JNICALL 
VMI_GetVMIFromJNIEnv(JNIEnv* env)
{
	JavaVM* javaVM;

	jint rc = (*env)->GetJavaVM(env, &javaVM);
	if (0 != rc) {
		return NULL;
	}

	return VMI_GetVMIFromJavaVM(javaVM);
}	

