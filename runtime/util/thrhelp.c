/*******************************************************************************
 * Copyright IBM Corp. and others 2015
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "j9.h"
#include "omrthread.h"
#include "util_api.h"
#include "ut_j9util.h"

J9VMThread *
getVMThreadFromOMRThread(J9JavaVM *vm, omrthread_t omrthread)
{
	J9VMThread *vmThread = NULL;

	if ((NULL != omrthread) && (NULL != vm->omrVM) && (vm->omrVM->_vmThreadKey > 0)) {
		OMR_VMThread *omrVMThread = ((OMR_VMThread *)omrthread_tls_get(omrthread, vm->omrVM->_vmThreadKey));
		if (NULL != omrVMThread) {
			vmThread = (J9VMThread *)omrVMThread->_language_vmthread;
		}
	}
	return vmThread;
}

void
initializeCurrentOSStackFree(J9VMThread *currentThread, omrthread_t osThread, UDATA osStackSize)
{
	UDATA actualStackSize = 0;
	UDATA stackStart = 0;
	UDATA stackEnd = 0;

	if (J9THREAD_SUCCESS == omrthread_get_stack_range(osThread, (void**)&stackStart, (void **)&stackEnd)) {
		actualStackSize = stackEnd - stackStart;
		currentThread->currentOSStackFree = ((UDATA)&stackStart - stackStart);
	} else {
		UDATA osStackFree = omrthread_current_stack_free();
		if (0 != osStackFree) {
			currentThread->currentOSStackFree = osStackFree - (osStackFree / J9VMTHREAD_RESERVED_C_STACK_FRACTION);
		} else {
			currentThread->currentOSStackFree = osStackSize - (osStackSize / J9VMTHREAD_RESERVED_C_STACK_FRACTION);
		}
	}
	Trc_Util_thrhelp_initializeCurrentOSStackFree(currentThread, osThread, osStackSize, actualStackSize, currentThread->currentOSStackFree, &actualStackSize);
}
