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

#include <string.h>
#include "j9protos.h"
#include "j9consts.h"
#include "ut_j9vm.h"

static VMINLINE IDATA signalEvent(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey, UDATA interrupt);

/*
 * Signal an async event (optionally interrupting the thread) on a specific thread, or all threads if targetThread is NULL.
 *
 * Returns 0 on success, non-zero on failure.
 */
static VMINLINE IDATA signalEvent(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey, UDATA interrupt)
{
	IDATA rc = J9ASYNC_ERROR_INVALID_HANDLER_KEY;

	Trc_VM_J9SignalAsyncEvent_Entry(targetThread, handlerKey);

	if ((handlerKey >= 0) && (handlerKey < J9ASYNC_MAX_HANDLERS)) {
		if (vm->asyncEventHandlers[handlerKey].handler != NULL) {
			if (targetThread == NULL) {
				omrthread_monitor_enter(vm->vmThreadListMutex);

				targetThread = vm->mainThread;
				do {
					setAsyncEventFlags(targetThread, (((UDATA) 1) << handlerKey), interrupt);
				} while ((targetThread = targetThread->linkNext) != vm->mainThread);

				omrthread_monitor_exit(vm->vmThreadListMutex);
			} else {
				setAsyncEventFlags(targetThread, (((UDATA) 1) << handlerKey), interrupt);
			}

			rc = J9ASYNC_ERROR_NONE;
		}
	}

	Trc_VM_J9SignalAsyncEvent_Exit(rc);

	return rc;
}

/*
 * Register a new async event handler.
 *
 * Returns a non-negative handler key on success, or a negative number upon failure.
 */
IDATA 
J9RegisterAsyncEvent(J9JavaVM * vm, J9AsyncEventHandler eventHandler, void * userData)
{
	IDATA handlerKey;
	omrthread_monitor_t const asyncEventMutex = vm->asyncEventMutex;

	Trc_VM_J9RegisterAsyncEvent_Entry(eventHandler, userData);

	if (NULL != asyncEventMutex) {
		omrthread_monitor_enter(asyncEventMutex);
	}
	for (handlerKey = 0; handlerKey < J9ASYNC_MAX_HANDLERS; ++handlerKey) {
		J9AsyncEventRecord * record = &vm->asyncEventHandlers[handlerKey];

		if (record->handler == NULL) {
			/* No need to worry about write order/barriers here since the event cannot be posted until this function returns the key */
			record->handler = eventHandler;
			record->userData = userData;
			goto done;
		}
	}
	handlerKey = J9ASYNC_ERROR_NO_MORE_HANDLERS;
done:
	if (NULL != asyncEventMutex) {
		omrthread_monitor_exit(asyncEventMutex);
	}

	Trc_VM_J9RegisterAsyncEvent_Exit(handlerKey);

	return handlerKey;
}

/*
 * Unregister an async event handler.
 *
 * Returns 0 on success, non-zero on failure.
 */
IDATA 
J9UnregisterAsyncEvent(J9JavaVM * vm, IDATA handlerKey)
{
	IDATA rc = J9ASYNC_ERROR_NONE;
	J9VMThread * currentThread = NULL;
	UDATA initialized = (NULL != vm->mainThread);

	Trc_VM_J9UnregisterAsyncEvent_Entry(handlerKey);

	/* Quick check to see if the VM has been initialized.  If there are no threads, then no
	 * events need to be cancelled.
	 */
	if (initialized) {
		/* Acquire exclusive to guarantee that no new async events are fired (all events are fired with vm access) */

		currentThread = currentVMThread(vm);
		if ((currentThread == NULL) || ((currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) == 0)) {
			acquireExclusiveVMAccessFromExternalThread(vm);
			currentThread = NULL;
		} else {
			acquireExclusiveVMAccess(currentThread);
		}

		/* Remove the unregistered event from all threads (which performs all the necessary error checking) */

		rc = J9CancelAsyncEvent(vm, NULL, handlerKey);
	}
	if (rc == J9ASYNC_ERROR_NONE) {
		J9AsyncEventRecord * record = &vm->asyncEventHandlers[handlerKey];

		record->userData = NULL;
		record->handler = NULL;
	}

	if (initialized) {
		if (currentThread == NULL) {
			releaseExclusiveVMAccessFromExternalThread(vm);
		} else {
			releaseExclusiveVMAccess(currentThread);
		}
	}

	Trc_VM_J9UnregisterAsyncEvent_Exit(rc);

	return rc;
}

/*
 * Signal an async event on a specific thread, or all threads if targetThread is NULL.
 *
 * Returns 0 on success, non-zero on failure.
 */
IDATA 
J9SignalAsyncEvent(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey)
{
	return signalEvent(vm, targetThread, handlerKey, TRUE);
}

/*
 * Signal an async event (without interrupting the thread) on a specific thread, or all threads if targetThread is NULL.
 *
 * Returns 0 on success, non-zero on failure.
 */
IDATA 
J9SignalAsyncEventWithoutInterrupt(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey)
{
	return signalEvent(vm, targetThread, handlerKey, FALSE);
}

/*
 * Cancel an async event on a specific thread, or all threads if targetThread is NULL.
 *
 * Returns 0 on success, non-zero on failure.
 */
IDATA 
J9CancelAsyncEvent(J9JavaVM * vm, J9VMThread * targetThread, IDATA handlerKey)
{
	IDATA rc = J9ASYNC_ERROR_INVALID_HANDLER_KEY;

	Trc_VM_J9CancelAsyncEvent_Entry(targetThread, handlerKey);

	if ((handlerKey >= 0) && (handlerKey < J9ASYNC_MAX_HANDLERS)) {
		if (vm->asyncEventHandlers[handlerKey].handler != NULL) {
			if (targetThread == NULL) {
				omrthread_monitor_t const vmThreadListMutex = vm->vmThreadListMutex;
				if (NULL != vmThreadListMutex) {
					omrthread_monitor_enter(vmThreadListMutex);
				}
				targetThread = vm->mainThread;
				if (NULL != targetThread) {
					do {
						clearAsyncEventFlags(targetThread, (((UDATA) 1) << handlerKey));
					} while ((targetThread = targetThread->linkNext) != vm->mainThread);
				}
				if (NULL != vmThreadListMutex) {
					omrthread_monitor_exit(vmThreadListMutex);
				}
			} else {
				clearAsyncEventFlags(targetThread, (((UDATA) 1) << handlerKey));
			}

			rc = J9ASYNC_ERROR_NONE;
		}
	}

	Trc_VM_J9CancelAsyncEvent_Exit(rc);

	return rc;
}

/*
 * Called from the async message handler once the async event flags have been fetched and zeroed.
 * Current thread has VM access.
 */
void 
dispatchAsyncEvents(J9VMThread * currentThread, UDATA asyncEventFlags)
{
	IDATA handlerKey = 0;
	J9AsyncEventRecord * record = currentThread->javaVM->asyncEventHandlers;

	Trc_VM_dispatchAsyncEvents_Entry(currentThread, asyncEventFlags);
	Assert_VM_mustHaveVMAccess(currentThread);

	/* Assembler code checks for all 0 before calling, so do the 0 check at the end */

	do {
		if (asyncEventFlags & 1) {
			J9AsyncEventHandler eventHandler = record->handler;

			if (eventHandler != NULL) {
				Trc_VM_dispatchAsyncEvents_callEventHandler(currentThread, handlerKey, eventHandler, record->userData);
				eventHandler(currentThread, handlerKey, record->userData);
			}
		}
		++record;
		++handlerKey;
		asyncEventFlags >>= 1;
	} while (asyncEventFlags != 0);

	Trc_VM_dispatchAsyncEvents_Exit(currentThread);
}

