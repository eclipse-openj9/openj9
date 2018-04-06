/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include <string.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "omrutil.h"

char*
getVMThreadNameFromString(J9VMThread *vmThread, j9object_t nameObject)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	char* result = javaVM->internalVMFunctions->copyStringToUTF8WithMemAlloc(vmThread, nameObject, J9_STR_NULL_TERMINATE_RESULT, "", 0, NULL, 0, NULL);

	return result;
}

/*
 * This API is only used to implement java.lang.Thread.setName().
 * Thread.setName() can be invoked on a thread other than the current thread.
 */
IDATA
setVMThreadNameFromString(J9VMThread *currentThread, J9VMThread *vmThread, j9object_t nameObject)
{
	char *name = getVMThreadNameFromString(currentThread, nameObject);
	if(name != NULL) {
		setOMRVMThreadNameWithFlag(currentThread->omrVMThread, vmThread->omrVMThread, name, 0);
#if defined(J9VM_THR_ASYNC_NAME_UPDATE)
		if (currentThread != vmThread) {
			J9JavaVM *vm = currentThread->javaVM;
			vm->internalVMFunctions->J9SignalAsyncEvent(vm, vmThread, vm->threadNameHandlerKey);
		} else
#endif /* J9VM_THR_ASYNC_NAME_UPDATE */
		{
#if defined(LINUX)
			pid_t pid = getpid();
			UDATA tid = omrthread_get_ras_tid();
			if ((UDATA)pid == tid) {
				return 0;
			}
#endif /* defined(LINUX) */
			omrthread_set_name(vmThread->osThread, name);
		}
		return 0;
	}
	return -1;
}

/*
 * This wrapper function is maintained for the JIT. The JIT uses nameIsStatic=1.
 */
void
setVMThreadNameWithFlag(J9VMThread *currentThread, J9VMThread *vmThread, char* name, U_8 nameIsStatic)
{
	setOMRVMThreadNameWithFlag(currentThread->omrVMThread, vmThread->omrVMThread, name, nameIsStatic);
}
