/*******************************************************************************
 * Copyright (c) 2011, 2014 IBM Corp. and others
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

#include "j9.h"
#include "util_api.h"

#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
#include "edcwccwi.h"
#endif

#define ZOS_V1R10_RELEASE	20.00
#define ZOS_V1R10_VERSION	3

/**
 * Register the J9VMThread->systemStackPointer field with the operating system.
 * This function needs to be called by the running thread on itself as the OS
 * registers a link between this J9VMThread's stack pointer register and the
 * native thread.  This allows the JIT to use the system stack pointer as a
 * general purpose register while also allowing the OS to still have the
 * necessary access to the C stack.
 */
void registerSystemStackPointerThreadOffset(J9VMThread *currentThread)
{
#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
	currentThread->systemStackPointer = 0;

	if (zos_version_at_least(ZOS_V1R10_RELEASE, ZOS_V1R10_VERSION)) {
		*__LE_SAVSTACK_ASYNC_ADDR = &(currentThread->systemStackPointer);
	}
#endif
}
