/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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

#include "j9protos.h"
#include "vm_internal.h"

#if defined(J9VM_OPT_JFR)

extern "C" {

#undef DEBUG

// TODO: allow different buffer sizes on different threads
#define J9JFR_THREAD_BUFFER_SIZE (1024*1024)

static void tearDownJFR(J9JavaVM *vm);
static bool flushBufferToGlobal(J9VMThread *currentThread, J9VMThread *flushThread);
static bool flushAllThreadBuffers(J9VMThread *currentThread);
static U_8* reserveBuffer(J9VMThread *currentThread, UDATA size);
static void jfrThreadCreated(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrThreadDestroy(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrClassesUnload(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrVMShutdown(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void initializeEventFields(J9VMThread *currentThread, J9JFREvent *event, UDATA eventType, UDATA threadState);

/**
 * Flush a thread local buffer to the global buffer.
 *
 * The flushThread parameter must be either the current thread or be
 * paused (e.g. by exclusive VM access).
 *
 * @param currentThread[in] the current J9VMThread
 * @param flushThread[in] the J9VMThread to fluah
 *
 * @returns true on success, false on failure
 */
static bool
flushBufferToGlobal(J9VMThread *currentThread, J9VMThread *flushThread)
{
#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! flushing %p of size %p\n", flushThread, flushThread->jfrData.bufferSize - flushThread->jfrData.bufferRemaining);
#endif /* defined(DEBUG) */

	// TODO: flush to global

	/* Reset the buffer */
	flushThread->jfrData.bufferRemaining = flushThread->jfrData.bufferSize;
	flushThread->jfrData.bufferCurrent = flushThread->jfrData.bufferStart;

	return true;
}

/**
 * Flush all thread local buffers to the global buffer.
 *
 * The current thread must have exclusive VM access.
 *
 * @param currentThread[in] the current J9VMThread
 *
 * @returns true if all buffers flushed successfully, false if not
 */
static bool
flushAllThreadBuffers(J9VMThread *currentThread)
{
	J9VMThread *loopThread = currentThread;
	bool allSucceeded = true;

	do {
		if (!flushBufferToGlobal(currentThread, loopThread)) {
			allSucceeded = false;
		}
		loopThread = loopThread->linkNext;
	} while (loopThread != currentThread);

	return allSucceeded;
}

/**
 * Reserve space in the local buffer of the current thread.
 *
 * @param currentThread[in] the current J9VMThread
 * @param size[in] the number of bytes to reserve
 *
 * @returns pointer to the start of the reserved space or NULL if the space could not be reserved
 */
static U_8*
reserveBuffer(J9VMThread *currentThread, UDATA size)
{
	U_8 *event = NULL;

	/* If the event is larger than the buffer, fail without attemptiong to flush */
	if (size <= currentThread->jfrData.bufferSize) {
		/* If there isn't enough space, flush the thread buffer to global */
		if (size > currentThread->jfrData.bufferRemaining) {
			if (!flushBufferToGlobal(currentThread, currentThread)) {
				goto done;
			}
		}
		event = currentThread->jfrData.bufferCurrent;
		currentThread->jfrData.bufferCurrent += size;
		currentThread->jfrData.bufferRemaining -= size;
	}
done:
	return event;
}

/**
 * Hook for new thread being created.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrThreadCreated(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThreadCreatedEvent *event = (J9VMThreadCreatedEvent *)eventData;
	J9VMThread *currentThread = event->vmThread;
	PORT_ACCESS_FROM_VMC(currentThread);

	// TODO: allow different buffer sizes on different threads

	U_8 *buffer = (U_8*)j9mem_allocate_memory(J9JFR_THREAD_BUFFER_SIZE, OMRMEM_CATEGORY_VM);
	if (NULL == buffer) {
		event->continueInitialization = FALSE;
	} else {
		currentThread->jfrData.bufferSize = J9JFR_THREAD_BUFFER_SIZE;
		currentThread->jfrData.bufferRemaining = J9JFR_THREAD_BUFFER_SIZE;
		currentThread->jfrData.bufferStart = buffer;
		currentThread->jfrData.bufferCurrent = buffer;
	}
}

/**
 * Hook for thread being destroyed.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrThreadDestroy(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThreadDestroyEvent *event = (J9VMThreadDestroyEvent *)eventData;
	J9VMThread *currentThread = event->vmThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! thread end %p\n", currentThread);
#endif /* defined(DEBUG) */

	/* Flush and free the thread local buffer */
	flushBufferToGlobal(currentThread, currentThread);
	j9mem_free_memory(currentThread->jfrData.bufferStart);
}

/**
 * Hook for classes being unloaded.
 *
 * Fkushes all thread buffers. Current thread has exclusive VM access.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrClassesUnload(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMClassesUnloadEvent *event = (J9VMClassesUnloadEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! class unload %p\n", currentThread);
#endif /* defined(DEBUG) */

	flushAllThreadBuffers(currentThread);
}

/**
 * Hook for VM shutting down.
 *
 * Fkushes and frees the thread local buffer.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrVMShutdown(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMShutdownEvent *event = (J9VMShutdownEvent *)eventData;
	J9VMThread *currentThread = event->vmThread;
	bool acquiredExclusive = false;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! shutdown %p\n", currentThread);
#endif /* defined(DEBUG) */

	if (J9_XACCESS_NONE == currentThread->javaVM->exclusiveAccessState) {
		internalAcquireVMAccess(currentThread);
		acquireExclusiveVMAccess(currentThread);
		acquiredExclusive = true;
	}

	flushAllThreadBuffers(currentThread);

	if (acquiredExclusive) {
		releaseExclusiveVMAccess(currentThread);
		internalReleaseVMAccess(currentThread);
	}

	tearDownJFR(currentThread->javaVM);
}

jint
initializeJFR(J9JavaVM *vm)
{
	jint rc = JNI_ERR;
	J9HookInterface **vmHooks = getVMHookInterface(vm);

	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_DESTROY, jfrThreadDestroy, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jfrClassesUnload, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrVMShutdown, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}

	// TODO: allocate global structures

	rc = JNI_OK;
done:
	return rc;
fail:
	tearDownJFR(vm);
	goto done;
}

/**
 * Shut down JFR.
 *
 * @param vm[in] the J9JavaVM
 */
static void
tearDownJFR(J9JavaVM *vm)
{
	J9HookInterface **vmHooks = getVMHookInterface(vm);

	// TODO: free global structures

	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_DESTROY, jfrThreadDestroy, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jfrClassesUnload, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrVMShutdown, NULL);
}

/**
 * Fill in the common fields of a JFR event
 *
 * @param currentThread[in] the current J9VMThread
 * @param event[in] pointer to the event structure
 * @param eventType[in] the event type
 * @param threadState[in] the thread state
 */
static void
initializeEventFields(J9VMThread *currentThread, J9JFREvent *event, UDATA eventType, UDATA threadState)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	event->time = j9time_current_time_millis();
	event->eventType = eventType;
	event->threadState = threadState;
	event->vmThread = currentThread;
}

void
jfrExecutionSample(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9StackWalkState *walkState = currentThread->stackWalkState;

	walkState->walkThread = currentThread;
	walkState->flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC |
			J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_SKIP_INLINES;
	walkState->skipCount = 0;
	UDATA walkRC = vm->walkStackFrames(currentThread, walkState);
	if (J9_STACKWALK_RC_NONE == walkRC) {
		UDATA framesWalked = walkState->framesWalked;
		UDATA stackTraceBytes = framesWalked * sizeof(UDATA);
		UDATA eventSize = sizeof(J9JFRExecutionSample) + stackTraceBytes;
		J9JFRExecutionSample *sample = (J9JFRExecutionSample*)reserveBuffer(currentThread, eventSize);
		if (NULL != sample) {
			initializeEventFields(currentThread, (J9JFREvent*)sample, J9JFR_EVENT_TYPE_EXECUTION_SAMPLE, J9JFR_THREAD_STATE_RUNNING);
			sample->stackTraceSize = framesWalked;
			memcpy(J9JFREXECUTIONSAMPLE_STACKTRACE(sample), walkState->cache, stackTraceBytes);
		}
		freeStackWalkCaches(currentThread, walkState);
	} else {
		// TODO: What to do when out of native memory?
	}
}

} /* extern "C" */

#endif /* defined(J9VM_OPT_JFR) */
