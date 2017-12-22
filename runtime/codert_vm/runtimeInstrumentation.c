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

#include "runtimeInstrumentation.h"

/**
 * Handler for the async message to update the RI fields in a thread.
 * 
 * @param[in] currentThread The current J9VMThread
 * @param[in] handlerKey Handler key of the current async event
 * @param[in] userData The J9JavaVM pointer
 */
static void
riAsyncHandler(J9VMThread *currentThread, IDATA handlerKey, void *userData)
{
	/**
	 * Update the current flag values.  There is no need to take action based on the new flags.
	 * The RI state will be updated on the next transition back to compiled code. 
	 */
	currentThread->jitCurrentRIFlags = currentThread->jitPendingRIFlags;
}

UDATA
initializeJITRuntimeInstrumentation(J9JavaVM *vm)
{
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	IDATA handlerKey = vmFuncs->J9RegisterAsyncEvent(vm, riAsyncHandler, vm);
	UDATA rc = 1;

	if (handlerKey >= 0) {
		vm->jitRIHandlerKey = handlerKey;
		rc = 0;
	}

	return rc;
}

void
shutdownJITRuntimeInstrumentation(J9JavaVM *vm)
{
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vmFuncs->J9UnregisterAsyncEvent(vm, vm->jitRIHandlerKey);
	vm->jitRIHandlerKey = -1;
}

UDATA
updateJITRuntimeInstrumentationFlags(J9VMThread *targetThread, UDATA newFlags)
{
	J9JavaVM *vm = targetThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA rc = 1;

	if (targetThread->privateFlags & J9_PRIVATE_FLAGS_RI_INITIALIZED) {
		targetThread->jitPendingRIFlags = newFlags;
		vmFuncs->J9SignalAsyncEvent(vm, targetThread, vm->jitRIHandlerKey);
		rc = 0;
	}

	return rc;
}
