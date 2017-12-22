/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include "j9port.h"
#include "rommeth.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "j9cp.h"
#include "j9consts.h"
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

extern "C" {

void
clearAsyncEventFlags(J9VMThread *vmThread, UDATA flags)
{
	VM_AtomicSupport::bitAnd(&vmThread->asyncEventFlags, ~flags);
}

void
setAsyncEventFlags(J9VMThread *vmThread, UDATA flags, UDATA indicateEvent)
{
	VM_AtomicSupport::bitOr(&vmThread->asyncEventFlags, flags);
	if (indicateEvent) {
		VM_VMHelpers::indicateAsyncMessagePending(vmThread);
	}
}

UDATA
javaCheckAsyncMessages(J9VMThread *currentThread, UDATA throwExceptions)
{
	UDATA result = J9_CHECK_ASYNC_NO_ACTION;
	/* Indicate that all current asyncs have been seen */
	currentThread->stackOverflowMark = currentThread->stackOverflowMark2;
	/* Process the hookable async events */
	VM_AtomicSupport::readBarrier();
	UDATA asyncEventFlags = VM_AtomicSupport::set(&currentThread->asyncEventFlags, 0);
	if (0 != asyncEventFlags) {
		dispatchAsyncEvents(currentThread, asyncEventFlags);
	}
	/* Start the async check loop */
	omrthread_monitor_enter(currentThread->publicFlagsMutex);
	UDATA volatile *flagsPtr = (UDATA volatile*)&currentThread->publicFlags;
	for (;;) {
		UDATA const publicFlags = *flagsPtr;
		/* Check for a pop frames request */
		if (J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) {
			/* Do not clear the drop flag yet (clear it once the drop is complete) */
			VM_VMHelpers::indicateAsyncMessagePending(currentThread);
			result = J9_CHECK_ASYNC_POP_FRAMES;
			break;
		}
		/* Check for a thread halt request */
		if (J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_RELEASE_ACCESS_REQUIRED_MASK)) {
			Assert_VM_false(J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT));
			internalReleaseVMAccessNoMutex(currentThread);
			internalAcquireVMAccessNoMutex(currentThread);
			continue;
		}
		/* Check for stop request.  Do this last so that the currentException does not get a chance
		 * to be overwritten when access is released.
		 */
		if (J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_STOP)) {
			if (throwExceptions) {
				currentThread->currentException = currentThread->stopThrowable;
				currentThread->stopThrowable = NULL;
				clearEventFlag(currentThread, J9_PUBLIC_FLAGS_STOP);
				omrthread_clear_priority_interrupted();
				result = J9_CHECK_ASYNC_THROW_EXCEPTION;
			} else {
				VM_VMHelpers::indicateAsyncMessagePending(currentThread);
			}
		}
		break;
	}
	omrthread_monitor_exit(currentThread->publicFlagsMutex);
	return result;
}

}
