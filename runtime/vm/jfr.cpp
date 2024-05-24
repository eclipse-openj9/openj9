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

// TODO: allow different buffer sizes on different threads and configureable sizes
#define J9JFR_THREAD_BUFFER_SIZE (1024*1024)
#define J9JFR_GLOBAL_BUFFER_SIZE (10 * J9JFR_THREAD_BUFFER_SIZE)

static UDATA jfrEventSize(J9JFREvent *jfrEvent);
static void tearDownJFR(J9JavaVM *vm);
static bool flushBufferToGlobal(J9VMThread *currentThread, J9VMThread *flushThread);
static bool flushAllThreadBuffers(J9VMThread *currentThread, bool freeBuffers);
static U_8* reserveBuffer(J9VMThread *currentThread, UDATA size);
static void jfrThreadCreated(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrThreadDestroy(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrClassesUnload(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrVMShutdown(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrThreadStarting(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void jfrThreadEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void initializeEventFields(J9VMThread *currentThread, J9JFREvent *jfrEvent, UDATA eventType);
static UDATA collectStackTrace(J9VMThread *currentThread, J9StackWalkState *walkState);

/**
 * Calculate the size in bytes of a JFR event.
 *
 * @param jfrEvent[in] pointer to the event
 *
 * @returns total size in bytes of the event
 */
static UDATA
jfrEventSize(J9JFREvent *jfrEvent)
{
	UDATA size = sizeof(J9JFREvent);
	switch(jfrEvent->eventType) {
	case J9JFR_EVENT_TYPE_EXECUTION_SAMPLE:
		size += (((J9JFRExecutionSample*)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	default:
		break;
	}
	return size;
}

J9JFREvent*
jfrBufferStartDo(J9JFRBuffer *buffer, J9JFRBufferWalkState *walkState)
{
	U_8 *start = buffer->bufferStart;
	U_8 *end = buffer->bufferCurrent;
	walkState->end = end;
	walkState->current = start;
	if (start == end) {
		start = NULL;
	}
	return (J9JFREvent*)start;
}

J9JFREvent*
jfrBufferNextDo(J9JFRBufferWalkState *walkState)
{
	U_8 *current = walkState->current;
	U_8 *next = NULL;
	if (walkState->end != current) {
		next = current + jfrEventSize((J9JFREvent*)current);
		walkState->current = next;
	}
	return (J9JFREvent*)next;
}

/**
 * Write out the contents of the global JFR buffer.
 *
 * The current thread must hold the jfrBufferMutex or have exclusive VM access.
 *
 * @param currentThread[in] the current J9VMThread
 *
 * @returns true on success, false on failure
 */
static bool
writeOutGlobalBuffer(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! writing global buffer %p of size %p\n", currentThread, vm->jfrBuffer.bufferSize - vm->jfrBuffer.bufferRemaining);
#endif /* defined(DEBUG) */

	// TODO: actually write the buffer

	/* Reset the buffer */
	vm->jfrBuffer.bufferRemaining = vm->jfrBuffer.bufferSize;
	vm->jfrBuffer.bufferCurrent = vm->jfrBuffer.bufferStart;

	return true;
}

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
	J9JavaVM *vm = currentThread->javaVM;
	UDATA bufferSize = flushThread->jfrBuffer.bufferSize - flushThread->jfrBuffer.bufferRemaining;
	bool success = true;
#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! flushing %p of size %p\n", flushThread, bufferSize);
#endif /* defined(DEBUG) */

	omrthread_monitor_enter(vm->jfrBufferMutex);
	if (vm->jfrBuffer.bufferRemaining < bufferSize) {
		if (!writeOutGlobalBuffer(currentThread)) {
			omrthread_monitor_exit(vm->jfrBufferMutex);
			success = false;
			goto done;
		}
	}
	memcpy(vm->jfrBuffer.bufferStart, flushThread->jfrBuffer.bufferStart, bufferSize);
	vm->jfrBuffer.bufferCurrent += bufferSize;
	vm->jfrBuffer.bufferRemaining -= bufferSize;
	omrthread_monitor_exit(vm->jfrBufferMutex);

	/* Reset the buffer */
	flushThread->jfrBuffer.bufferRemaining = flushThread->jfrBuffer.bufferSize;
	flushThread->jfrBuffer.bufferCurrent = flushThread->jfrBuffer.bufferStart;
done:
	return success;
}

/**
 * Flush all thread local buffers to the global buffer.
 *
 * The current thread must have exclusive VM access.
 *
 * @param currentThread[in] the current J9VMThread
 * @param freeBuffers[in] true to free the buffers after flush, false to retain the buffers
 *
 * @returns true if all buffers flushed successfully, false if not
 */
static bool
flushAllThreadBuffers(J9VMThread *currentThread, bool freeBuffers)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9VMThread *loopThread = currentThread;
	bool allSucceeded = true;

	do {
		if (!flushBufferToGlobal(currentThread, loopThread)) {
			allSucceeded = false;
		}
		if (freeBuffers) {
			j9mem_free_memory((void*)loopThread->jfrBuffer.bufferStart);
			memset(&loopThread->jfrBuffer, 0, sizeof(loopThread->jfrBuffer));
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
	U_8 *jfrEvent = NULL;

	/* If the event is larger than the buffer, fail without attemptiong to flush */
	if (size <= currentThread->jfrBuffer.bufferSize) {
		/* If there isn't enough space, flush the thread buffer to global */
		if (size > currentThread->jfrBuffer.bufferRemaining) {
			if (!flushBufferToGlobal(currentThread, currentThread)) {
				goto done;
			}
		}
		jfrEvent = currentThread->jfrBuffer.bufferCurrent;
		currentThread->jfrBuffer.bufferCurrent += size;
		currentThread->jfrBuffer.bufferRemaining -= size;
	}
done:
	return jfrEvent;
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
		currentThread->jfrBuffer.bufferStart = buffer;
		currentThread->jfrBuffer.bufferCurrent = buffer;
		currentThread->jfrBuffer.bufferSize = J9JFR_THREAD_BUFFER_SIZE;
		currentThread->jfrBuffer.bufferRemaining = J9JFR_THREAD_BUFFER_SIZE;
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


	/* The current thread pointer (which can appear in other thread buffers) is about to become
	 * invalid, so write out all of the available data now.
	 */
	flushAllThreadBuffers(currentThread, false);
	writeOutGlobalBuffer(currentThread);

	/* Free the thread local buffer */
	j9mem_free_memory((void*)currentThread->jfrBuffer.bufferStart);
	memset(&currentThread->jfrBuffer, 0, sizeof(currentThread->jfrBuffer));
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

	/* Some class pointers in the thread and global buffers are about the become
	 * invalid, so write out all of the available data now.
	 */
	flushAllThreadBuffers(currentThread, false);
	writeOutGlobalBuffer(currentThread);
}

/**
 * Hook for VM shutting down.
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

	/* Flush and free all the thread buffers and write out the global buffer */
	flushAllThreadBuffers(currentThread, true);
	writeOutGlobalBuffer(currentThread);

	if (acquiredExclusive) {
		releaseExclusiveVMAccess(currentThread);
		internalReleaseVMAccess(currentThread);
	}

	tearDownJFR(currentThread->javaVM);
}

/**
 * Hook for thread starting.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrThreadStarting(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThreadStartingEvent *event = (J9VMThreadStartingEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	J9VMThread *startedThread = event->startedThread;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! thread starting %p %p\n", currentThread, startedThread);
#endif /* defined(DEBUG) */

	J9StackWalkState *walkState = currentThread->stackWalkState;
	UDATA walkRC = collectStackTrace(currentThread, walkState);
	if (J9_STACKWALK_RC_NONE == walkRC) {
		UDATA framesWalked = walkState->framesWalked;
		UDATA stackTraceBytes = framesWalked * sizeof(UDATA);
		UDATA eventSize = sizeof(J9JFRThreadStart) + stackTraceBytes;
		J9JFRThreadStart *jfrEvent = (J9JFRThreadStart*)reserveBuffer(currentThread, eventSize);
		if (NULL != jfrEvent) {
			initializeEventFields(currentThread, (J9JFREvent*)jfrEvent, J9JFR_EVENT_TYPE_THREAD_START);
			jfrEvent->thread = startedThread;
			jfrEvent->parentThread = currentThread;
			jfrEvent->stackTraceSize = framesWalked;
			memcpy(J9JFRTHREADSTART_STACKTRACE(jfrEvent), walkState->cache, stackTraceBytes);
		}
		freeStackWalkCaches(currentThread, walkState);
	}
}

/**
 * Hook for thread ending.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrThreadEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThreadEndEvent *event = (J9VMThreadEndEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! thread end %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFREvent *jfrEvent = (J9JFREvent*)reserveBuffer(currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, jfrEvent, J9JFR_EVENT_TYPE_THREAD_END);
	}
}

jint
initializeJFR(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rc = JNI_ERR;
	J9HookInterface **vmHooks = getVMHookInterface(vm);
	U_8 *buffer = NULL;

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
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_STARTING, jfrThreadStarting, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_END, jfrThreadEnd, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}

	/* Allocate the global buffer and mutex */
	buffer = (U_8*)j9mem_allocate_memory(J9JFR_GLOBAL_BUFFER_SIZE, OMRMEM_CATEGORY_VM);
	if (NULL == buffer) {
		goto fail;
	}
	vm->jfrBuffer.bufferStart = buffer;
	vm->jfrBuffer.bufferCurrent = buffer;
	vm->jfrBuffer.bufferSize = J9JFR_GLOBAL_BUFFER_SIZE;
	vm->jfrBuffer.bufferRemaining = J9JFR_GLOBAL_BUFFER_SIZE;
	if (omrthread_monitor_init_with_name(&vm->jfrBufferMutex, 0, "JFR global buffer mutex")) {
		goto done;
	}

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
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9HookInterface **vmHooks = getVMHookInterface(vm);

	/* Unhook all the events */
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_DESTROY, jfrThreadDestroy, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jfrClassesUnload, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrVMShutdown, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_STARTING, jfrThreadStarting, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_END, jfrThreadEnd, NULL);

	/* Free global buffer and mutex */

	j9mem_free_memory((void*)vm->jfrBuffer.bufferStart);
	memset(&vm->jfrBuffer, 0, sizeof(vm->jfrBuffer));
	if (NULL != vm->jfrBufferMutex) {
		omrthread_monitor_destroy(vm->jfrBufferMutex);
		vm->jfrBufferMutex = NULL;
	}
}

/**
 * Fill in the common fields of a JFR event
 *
 * @param currentThread[in] the current J9VMThread
 * @param event[in] pointer to the event structure
 * @param eventType[in] the event type
 */
static void
initializeEventFields(J9VMThread *currentThread, J9JFREvent *event, UDATA eventType)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	event->time = j9time_current_time_millis();
	event->eventType = eventType;
	event->vmThread = currentThread;
}

void
jfrExecutionSample(J9VMThread *currentThread)
{
	J9StackWalkState *walkState = currentThread->stackWalkState;
	UDATA walkRC = collectStackTrace(currentThread, walkState);
	if (J9_STACKWALK_RC_NONE == walkRC) {
		UDATA framesWalked = walkState->framesWalked;
		UDATA stackTraceBytes = framesWalked * sizeof(UDATA);
		UDATA eventSize = sizeof(J9JFRExecutionSample) + stackTraceBytes;
		J9JFRExecutionSample *jfrEvent = (J9JFRExecutionSample*)reserveBuffer(currentThread, eventSize);
		if (NULL != jfrEvent) {
			initializeEventFields(currentThread, (J9JFREvent*)jfrEvent, J9JFR_EVENT_TYPE_EXECUTION_SAMPLE);
			jfrEvent->threadState = J9JFR_THREAD_STATE_RUNNING;
			jfrEvent->stackTraceSize = framesWalked;
			memcpy(J9JFREXECUTIONSAMPLE_STACKTRACE(jfrEvent), walkState->cache, stackTraceBytes);
		}
		freeStackWalkCaches(currentThread, walkState);
	}
}

static UDATA
collectStackTrace(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	walkState->walkThread = currentThread;
	walkState->flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC |
			J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_SKIP_INLINES;
	walkState->skipCount = 0;
	UDATA walkRC = currentThread->javaVM->walkStackFrames(currentThread, walkState);
	if (J9_STACKWALK_RC_NONE != walkRC) {
		// TODO: tracepoint
	}
	return walkRC;
}

} /* extern "C" */

#endif /* defined(J9VM_OPT_JFR) */
