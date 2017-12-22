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

#include "vmi.h"
#include "util_api.h"

/**
 * Retrieves a VMInterface pointer given a JavaVM.
 * @param vm The VM instance from which to obtain the VMI.
 * @return  A VMInterface or NULL.
 */
VMInterface *JNICALL GetVMIFromJavaVM(JavaVM * vm)
{
	return (VMInterface*)&(((J9JavaVM*)vm)->vmInterface);
}

/**
 * Retrieves a VMInterface pointer given a JNIEnv.
 * @param env The JNIEnv from which to obtain the VMI.
 * @return  A VMInterface or NULL.
 */
VMInterface *JNICALL GetVMIFromJNIEnv (JNIEnv * env)
{
	return (VMInterface*)&(((J9VMThread*)env)->javaVM->vmInterface);
}
