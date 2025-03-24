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
#include "JFRConstantPoolTypes.hpp"
#include "j9protos.h"
#include "omrlinkedlist.h"
#include "pool_api.h"
#include "thread_api.h"
#include "ut_j9vm.h"
#include "vm_internal.h"

#if defined(J9VM_OPT_JFR)

#include "AtomicSupport.hpp"
#include "JFRWriter.hpp"

extern "C" {

typedef struct J9JFRTypeID {
	jlong id;
	J9Class *clazz;
} J9JFRTypeID;

#undef DEBUG

// TODO: allow configureable values
#define J9JFR_THREAD_BUFFER_SIZE (1024*1024)
#define J9JFR_GLOBAL_BUFFER_SIZE (10 * J9JFR_THREAD_BUFFER_SIZE)
#define J9JFR_SAMPLING_RATE 10

/* Value needs to be the same as jdk.jfr.internal.JVM.RESERVED_CLASS_ID_LIMIT. */
#define RESERVED_CLASS_ID_LIMIT 500

#define INVALID_TYPE_ID -1
#define BOOLEAN_TYPE_ID 1
#define BYTE_TYPE_ID 2
#define CHAR_TYPE_ID 3
#define SHORT_TYPE_ID 4
#define INT_TYPE_ID 5
#define FLOAT_TYPE_ID 6
#define LONG_TYPE_ID 7
#define DOUBLE_TYPE_ID 8
#define STACKTRACE_TYPE_ID 9

static void jfrStartSamplingThread(J9JavaVM *vm);
static void initializeEventFields(J9VMThread *currentThread, J9JFREvent *jfrEvent, UDATA eventType);
static int J9THREAD_PROC jfrSamplingThreadProc(void *entryArg);
static void jfrExecutionSampleCallback(J9VMThread *currentThread, IDATA handlerKey, void *userData);
static void jfrThreadCPULoadCallback(J9VMThread *currentThread, IDATA handlerKey, void *userData);

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
	UDATA size = 0;
	switch(jfrEvent->eventType) {
	case J9JFR_EVENT_TYPE_EXECUTION_SAMPLE:
		size = sizeof(J9JFRExecutionSample) + (((J9JFRExecutionSample*)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_THREAD_START:
		size = sizeof(J9JFRThreadStart) + (((J9JFRThreadStart*)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_THREAD_END:
		size = sizeof(J9JFREvent);
		break;
	case J9JFR_EVENT_TYPE_THREAD_SLEEP:
		size = sizeof(J9JFRThreadSlept) + (((J9JFRThreadSlept*)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_OBJECT_WAIT:
		size = sizeof(J9JFRMonitorWaited) + (((J9JFRMonitorWaited*)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_MONITOR_ENTER:
		size = sizeof(J9JFRMonitorEntered) + (((J9JFRMonitorEntered *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_THREAD_PARK:
		size = sizeof(J9JFRThreadParked) + (((J9JFRThreadParked*)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_CPU_LOAD:
		size = sizeof(J9JFRCPULoad);
		break;
	case J9JFR_EVENT_TYPE_THREAD_CPU_LOAD:
		size = sizeof(J9JFRThreadCPULoad);
		break;
	case J9JFR_EVENT_TYPE_CLASS_LOADING_STATISTICS:
		size = sizeof(J9JFRClassLoadingStatistics);
		break;
	case J9JFR_EVENT_TYPE_THREAD_CONTEXT_SWITCH_RATE:
		size = sizeof(J9JFRThreadContextSwitchRate);
		break;
	case J9JFR_EVENT_TYPE_THREAD_STATISTICS:
		size = sizeof(J9JFRThreadStatistics);
		break;
	default:
		Assert_VM_unreachable();
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
	U_8 *next = current + jfrEventSize((J9JFREvent*)current);

	Assert_VM_true(walkState->end >= next);

	if (walkState->end == next) {
		next = NULL;
	}

	walkState->current = (U_8*)next;

	return (J9JFREvent*)next;
}

static bool
areJFRBuffersReadyForWrite(J9VMThread *currentThread)
{
	bool result = true;
	J9JavaVM *vm = currentThread->javaVM;

	if ((!vm->jfrState.isStarted)
	|| (NULL == currentThread->jfrBuffer.bufferStart)
	|| (NULL == vm->jfrBuffer.bufferCurrent)
	) {
		result = false;
	}

	return result;
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
writeOutGlobalBuffer(J9VMThread *currentThread, bool finalWrite, bool dumpCalled)
{
	J9JavaVM *vm = currentThread->javaVM;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! writing global buffer %p of size %p\n", currentThread, vm->jfrBuffer.bufferSize - vm->jfrBuffer.bufferRemaining);
#endif /* defined(DEBUG) */

	if (vm->jfrState.isStarted && (NULL != vm->jfrBuffer.bufferCurrent)) {
		VM_JFRWriter::flushJFRDataToFile(currentThread, finalWrite, dumpCalled);

		/* Reset the buffer */
		vm->jfrBuffer.bufferRemaining = vm->jfrBuffer.bufferSize;
		vm->jfrBuffer.bufferCurrent = vm->jfrBuffer.bufferStart;


#if defined(DEBUG)
		memset(vm->jfrBuffer.bufferStart, 0, J9JFR_GLOBAL_BUFFER_SIZE);
#endif /* defined(DEBUG) */
	}

	return true;
}

/**
 * Flush a thread local buffer to the global buffer.
 *
 * The flushThread parameter must be either the current thread or be
 * paused (e.g. by exclusive VM access).
 *
 * @param currentThread[in] the current J9VMThread
 * @param flushThread[in] the J9VMThread to flush
 *
 * @returns true on success, false on failure
 */
static bool
flushBufferToGlobal(J9VMThread *currentThread, J9VMThread *flushThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA bufferSize = flushThread->jfrBuffer.bufferCurrent - flushThread->jfrBuffer.bufferStart;
	bool success = true;

	if (!areJFRBuffersReadyForWrite(currentThread)) {
		goto done;
	}

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! flushing %p of size %u start=%p current=%p\n", flushThread, (U_32)bufferSize, flushThread->jfrBuffer.bufferStart, flushThread->jfrBuffer.bufferCurrent);
#endif /* defined(DEBUG) */

	omrthread_monitor_enter(vm->jfrBufferMutex);
	if (vm->jfrBuffer.bufferRemaining < bufferSize) {
		if (!writeOutGlobalBuffer(currentThread, false, false)) {
			omrthread_monitor_exit(vm->jfrBufferMutex);
			success = false;
			goto done;
		}
	}
	memcpy(vm->jfrBuffer.bufferCurrent, flushThread->jfrBuffer.bufferStart, bufferSize);
	vm->jfrBuffer.bufferCurrent += bufferSize;
	vm->jfrBuffer.bufferRemaining -= bufferSize;
	omrthread_monitor_exit(vm->jfrBufferMutex);

	/* Reset the buffer */
	flushThread->jfrBuffer.bufferRemaining = flushThread->jfrBuffer.bufferSize;
	flushThread->jfrBuffer.bufferCurrent = flushThread->jfrBuffer.bufferStart;

#if defined(DEBUG)
	memset(flushThread->jfrBuffer.bufferStart, 0, J9JFR_THREAD_BUFFER_SIZE);
#endif /* defined(DEBUG) */

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
	J9JavaVM *vm = currentThread->javaVM;
	J9VMThread *loopThread = vm->mainThread;
	bool allSucceeded = true;
	bool flushedCurrentThread = false;

	Assert_VM_mustHaveVMAccess(currentThread);
	Assert_VM_true(currentThread->omrVMThread->exclusiveCount > 0);
	Assert_VM_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));

	do {
		if (!flushBufferToGlobal(currentThread, loopThread)) {
			allSucceeded = false;
		}
		if (freeBuffers) {
			j9mem_free_memory((void*)loopThread->jfrBuffer.bufferStart);
			memset(&loopThread->jfrBuffer, 0, sizeof(loopThread->jfrBuffer));
		}

		if (loopThread == currentThread) {
			flushedCurrentThread = true;
		}

		loopThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, loopThread);
	} while (loopThread != NULL);

	if (!flushedCurrentThread) {
		/* current thread will not be in thread list */
		if (!flushBufferToGlobal(currentThread, currentThread)) {
			allSucceeded = false;
		}
		if (freeBuffers) {
			j9mem_free_memory((void*)currentThread->jfrBuffer.bufferStart);
			memset(&currentThread->jfrBuffer, 0, sizeof(currentThread->jfrBuffer));
		}
	}

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
	J9JavaVM *vm = currentThread->javaVM;

	/* Either we are holding on to VM access or this operation is driven by another thread that has exclusive. */
	Assert_VM_true(((currentThread)->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS)
	|| ((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState)));

	if (!areJFRBuffersReadyForWrite(currentThread)) {
		goto done;
	}

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
 * Reserve space for an event, initialize the common fields
 * and attach the stack trace.
 *
 * @param currentThread[in] the current J9VMThread
 * @param eventType[in] the event type
 * @param eventFixedSize[in] size of the fixed portion of the event
 *
 * @returns pointer to the start of the reserved space or NULL if the space could not be reserved
 */
static J9JFREvent*
reserveBufferWithStackTrace(J9VMThread *currentThread, J9VMThread *sampleThread, UDATA eventType, UDATA eventFixedSize)
{
	J9JFREvent *jfrEvent = NULL;
	J9StackWalkState *walkState = currentThread->stackWalkState;
	walkState->walkThread = sampleThread;
	walkState->flags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC |
			J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_SKIP_INLINES;
	walkState->skipCount = 0;
	UDATA walkRC = currentThread->javaVM->walkStackFrames(currentThread, walkState);
	if (J9_STACKWALK_RC_NONE == walkRC) {
		UDATA framesWalked = walkState->framesWalked;
		UDATA stackTraceBytes = framesWalked * sizeof(UDATA);
		UDATA eventSize = eventFixedSize + stackTraceBytes;
		jfrEvent = (J9JFREvent*)reserveBuffer(sampleThread, eventSize);
		if (NULL != jfrEvent) {
			initializeEventFields(sampleThread, jfrEvent, eventType);
			((J9JFREventWithStackTrace*)jfrEvent)->stackTraceSize = framesWalked;
			memcpy(((U_8*)jfrEvent) + eventFixedSize, walkState->cache, stackTraceBytes);
		}
		freeStackWalkCaches(currentThread, walkState);

	} else {
		// TODO: tracepoint
	}
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

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! thread created  %p\n", currentThread);
#endif /* defined(DEBUG) */

	/* TODO: allow different buffer sizes on different threads. */
	U_8 *buffer = (U_8*)j9mem_allocate_memory(J9JFR_THREAD_BUFFER_SIZE, OMRMEM_CATEGORY_VM);
	if (NULL == buffer) {
		event->continueInitialization = FALSE;
	} else {
		currentThread->jfrBuffer.bufferStart = buffer;
		currentThread->jfrBuffer.bufferCurrent = buffer;
		currentThread->jfrBuffer.bufferSize = J9JFR_THREAD_BUFFER_SIZE;
		currentThread->jfrBuffer.bufferRemaining = J9JFR_THREAD_BUFFER_SIZE;
#if defined(DEBUG)
		memset(currentThread->jfrBuffer.bufferStart, 0, J9JFR_THREAD_BUFFER_SIZE);
#endif /* defined(DEBUG) */
	}
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
	writeOutGlobalBuffer(currentThread, false, false);
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
	bool needsVMAccess = J9_ARE_NO_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS);
	bool acquiredExclusive = false;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! shutdown %p\n", currentThread);
#endif /* defined(DEBUG) */

	if (needsVMAccess) {
		internalAcquireVMAccess(currentThread);
	}

	if (J9_XACCESS_NONE == currentThread->javaVM->exclusiveAccessState) {
		acquireExclusiveVMAccess(currentThread);
		acquiredExclusive = true;
	}

	/* Flush and free all the thread buffers and write out the global buffer */
	flushAllThreadBuffers(currentThread, true);
	writeOutGlobalBuffer(currentThread, true, true);

	if (acquiredExclusive) {
		releaseExclusiveVMAccess(currentThread);
	}

	tearDownJFR(currentThread->javaVM);

	if (needsVMAccess) {
		internalReleaseVMAccess(currentThread);
	}
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

	J9JFRThreadStart *jfrEvent = (J9JFRThreadStart*)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_THREAD_START, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		jfrEvent->thread = startedThread;
		jfrEvent->parentThread = currentThread;
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

	internalAcquireVMAccess(currentThread);
	J9JFREvent *jfrEvent = (J9JFREvent*)reserveBuffer(currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, jfrEvent, J9JFR_EVENT_TYPE_THREAD_END);
	}
	PORT_ACCESS_FROM_VMC(currentThread);
	acquireExclusiveVMAccess(currentThread);
	flushAllThreadBuffers(currentThread, false);
	writeOutGlobalBuffer(currentThread, false, false);

	/* Free the thread local buffer */
	j9mem_free_memory((void*)currentThread->jfrBuffer.bufferStart);
	memset(&currentThread->jfrBuffer, 0, sizeof(currentThread->jfrBuffer));
	releaseExclusiveVMAccess(currentThread);
	internalReleaseVMAccess(currentThread);
}

/**
 * Hook for thread about to sleep.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrVMSlept(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMSleptEvent *event = (J9VMSleptEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! thread sleep %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFRThreadSlept *jfrEvent = (J9JFRThreadSlept*)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_THREAD_SLEEP, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		// TODO: worry about overflow?
		jfrEvent->time = (event->millis * 1000000) + event->nanos;
		jfrEvent->duration = 0;
		I_64 currentNanos = j9time_nano_time();
		jfrEvent->duration = currentNanos - event->startTicks;
	}
}

/**
 * Hook for VM intialized. Called without VM access.
 *
 * @param hook[in] the VM hook interface, not used
 * @param eventNum[in] the event number, not used
 * @param eventData[in] the event data
 * @param userData[in] the registered user data, not used
 */
static void
jfrVMInitialized(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThread *currentThread = ((J9VMInitEvent *)eventData)->vmThread;

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! VM init %p\n", currentThread);
#endif /* defined(DEBUG) */
	jfrStartSamplingThread(currentThread->javaVM);
}

/**
 * Start JFR sampling thread. Called without VM access.
 *
 * @param vm[in] pointer to the J9JavaVM
 */
static void
jfrStartSamplingThread(J9JavaVM *vm)
{
	/* Start the sampler thread. */
	IDATA rc = omrthread_create(&(vm->jfrSamplerThread), vm->defaultOSStackSize, J9THREAD_PRIORITY_NORMAL, FALSE, jfrSamplingThreadProc, (void*)vm);
	if (0 == rc) {
		omrthread_monitor_enter(vm->jfrSamplerMutex);
		while (J9JFR_SAMPLER_STATE_UNINITIALIZED == vm->jfrSamplerState) {
			omrthread_monitor_wait(vm->jfrSamplerMutex);
		}
		omrthread_monitor_exit(vm->jfrSamplerMutex);
		Trc_VM_jfrStartSamplingThread_jfrSamplerState(vm->jfrSamplerState);
	} else {
		Trc_VM_jfrStartSamplingThread_omrthread_create_failed(rc);
	}
}

/**
 * Hook for VM monitor waited. Called without VM access.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrVMMonitorWaited(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMMonitorWaitedEvent *event = (J9VMMonitorWaitedEvent*) eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! VM monitor waited %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFRMonitorWaited *jfrEvent = (J9JFRMonitorWaited*)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_OBJECT_WAIT, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		jfrEvent->time = (event->millis * 1000000) + event->nanos;
		jfrEvent->duration = j9time_nano_time() - event->startTicks;
		jfrEvent->monitorClass = event->monitorClass;
		jfrEvent->monitorAddress = event->monitorAddress;
		jfrEvent->timedOut = (J9THREAD_TIMED_OUT == event->reason);
	}
}

/**
 * Hook for VM monitor entered. Called without VM access.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrVMMonitorEntered(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMMonitorContendedEnteredEvent *event = (J9VMMonitorContendedEnteredEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! VM monitor entered %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFRMonitorEntered *jfrEvent = (J9JFRMonitorEntered *)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_MONITOR_ENTER, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_MONITOR_ENTER);

		jfrEvent->duration = j9time_nano_time() - event->startTicks;
		jfrEvent->monitorClass = event->monitorClass;
		jfrEvent->monitorAddress = (UDATA)event->monitor;
		jfrEvent->previousOwner = event->previousOwner;
	}
}

/**
 * Hook for VM thread parked. Called without VM access.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrVMThreadParked(J9HookInterface **hook, UDATA eventNum, void *eventData, void* userData)
{
	J9VMUnparkedEvent *event = (J9VMUnparkedEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! thread park %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFRThreadParked *jfrEvent = (J9JFRThreadParked*)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_THREAD_PARK, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		// TODO: worry about overflow?
		I_64 currentTime = j9time_nano_time();
		jfrEvent->duration = currentTime - event->startTicks;
		jfrEvent->parkedClass = event->parkedClass;
		jfrEvent->timeOut = (event->millis * 1000000) + event->nanos;
		jfrEvent->untilTime = currentTime;
		jfrEvent->parkedAddress = event->parkedAddress;
	}
}


jint
initializeJFR(J9JavaVM *vm, BOOLEAN lateInit)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);
	jint rc = JNI_ERR;
	J9HookInterface **vmHooks = getVMHookInterface(vm);
	U_8 *buffer = NULL;
	UDATA timeSuccess = 0;

	if (lateInit && vm->jfrState.isStarted) {
		Trc_VM_initializeJFR_isStarted();
		goto done;
	}

	/* Register async handler for execution samples. */
	vm->jfrAsyncKey = J9RegisterAsyncEvent(vm, jfrExecutionSampleCallback, NULL);
	if (vm->jfrAsyncKey < 0) {
		goto fail;
	}

	vm->jfrThreadCPULoadAsyncKey = J9RegisterAsyncEvent(vm, jfrThreadCPULoadCallback, NULL);
	if (vm->jfrThreadCPULoadAsyncKey < 0) {
		goto fail;
	}

	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, OMR_GET_CALLSITE(), NULL)) {
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
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SLEPT, jfrVMSlept, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if (!lateInit) {
		if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INITIALIZED, jfrVMInitialized, OMR_GET_CALLSITE(), NULL)) {
			goto fail;
		}
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_WAITED, jfrVMMonitorWaited, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, jfrVMMonitorEntered, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_UNPARKED, jfrVMThreadParked, OMR_GET_CALLSITE(), NULL)) {
		goto fail;
	}

	/* Allocate constantEvents. */
	vm->jfrState.constantEvents = j9mem_allocate_memory(sizeof(JFRConstantEvents), J9MEM_CATEGORY_VM);
	if (NULL == vm->jfrState.constantEvents) {
		goto fail;
	}
	memset(vm->jfrState.constantEvents, 0, sizeof(JFRConstantEvents));

	/* Allocate global data. */
	buffer = (U_8*)j9mem_allocate_memory(J9JFR_GLOBAL_BUFFER_SIZE, OMRMEM_CATEGORY_VM);
	if (NULL == buffer) {
		goto fail;
	}

#if defined(DEBUG)
	memset(buffer, 0, J9JFR_GLOBAL_BUFFER_SIZE);
#endif /* defined(DEBUG) */

	vm->jfrBuffer.bufferStart = buffer;
	vm->jfrBuffer.bufferCurrent = buffer;
	vm->jfrBuffer.bufferSize = J9JFR_GLOBAL_BUFFER_SIZE;
	vm->jfrBuffer.bufferRemaining = J9JFR_GLOBAL_BUFFER_SIZE;
	vm->jfrState.jfrChunkCount = 0;
	vm->jfrState.isConstantEventsInitialized = FALSE;

	vm->jfrState.chunkStartTime = (I_64) j9time_current_time_nanos(&timeSuccess);
	vm->jfrState.chunkStartTicks = j9time_nano_time();

	if (0 == timeSuccess) {
		goto fail;
	}

	vm->jfrState.prevSysCPUTime.timestamp = -1;
	vm->jfrState.prevProcTimestamp = -1;

	if (0 == omrsysinfo_get_number_context_switches(&vm->jfrState.prevContextSwitches)) {
		vm->jfrState.prevContextSwitchTimestamp = j9time_nano_time();
	} else {
		vm->jfrState.prevContextSwitchTimestamp = -1;
		vm->jfrState.prevContextSwitches = 0;
	}

	if (omrthread_monitor_init_with_name(&vm->jfrBufferMutex, 0, "JFR global buffer mutex")) {
		goto fail;
	}
	if (omrthread_monitor_init_with_name(&vm->jfrSamplerMutex, 0, "JFR sampler mutex")) {
		goto fail;
	}
	if (omrthread_monitor_init_with_name(&vm->jfrState.isConstantEventsInitializedMutex, 0, "Is JFR constantEvents initialized mutex")) {
		goto fail;
	}

	if (!VM_JFRWriter::initializaJFRWriter(vm)) {
		goto fail;
	}

	if (lateInit) {
		/* Go through existing threads. */
		J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
		while (NULL != walkThread) {
			/* only initialize a thread once */
			if (NULL == walkThread->jfrBuffer.bufferStart) {
				U_8 *buffer = (U_8*)j9mem_allocate_memory(J9JFR_THREAD_BUFFER_SIZE, OMRMEM_CATEGORY_VM);
				if (NULL == buffer) {
					goto fail;
				} else {
					walkThread->jfrBuffer.bufferStart = buffer;
					walkThread->jfrBuffer.bufferCurrent = buffer;
					walkThread->jfrBuffer.bufferSize = J9JFR_THREAD_BUFFER_SIZE;
					walkThread->jfrBuffer.bufferRemaining = J9JFR_THREAD_BUFFER_SIZE;
				}
			}

			walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
		}

		jfrStartSamplingThread(vm);
	}

done:
	vm->jfrState.isStarted = TRUE;
	rc = JNI_OK;
	return rc;
fail:
	tearDownJFR(vm);
	goto done;
}

void
tearDownJFR(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9VMThread *currentThread = currentVMThread(vm);
	J9HookInterface **vmHooks = getVMHookInterface(vm);

	Assert_VM_mustHaveVMAccess(currentThread);

	internalReleaseVMAccess(currentThread);

	/* Stop the sampler thread */
	if (NULL != vm->jfrSamplerMutex) {
		omrthread_monitor_enter(vm->jfrSamplerMutex);
		if (J9JFR_SAMPLER_STATE_RUNNING == vm->jfrSamplerState) {
			vm->jfrSamplerState = J9JFR_SAMPLER_STATE_STOP;
			omrthread_monitor_notify_all(vm->jfrSamplerMutex);
			while (J9JFR_SAMPLER_STATE_DEAD != vm->jfrSamplerState) {
				omrthread_monitor_wait(vm->jfrSamplerMutex);
			}
		}
		omrthread_monitor_exit(vm->jfrSamplerMutex);
		omrthread_monitor_destroy(vm->jfrSamplerMutex);
		vm->jfrSamplerMutex = NULL;
	}

	internalAcquireVMAccess(currentThread);

	vm->jfrState.isStarted = FALSE;
	vm->jfrSamplerState = J9JFR_SAMPLER_STATE_UNINITIALIZED;

	VM_JFRWriter::teardownJFRWriter(vm);

	/* Unhook all the events */
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jfrClassesUnload, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrVMShutdown, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_STARTING, jfrThreadStarting, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_END, jfrThreadEnd, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SLEPT, jfrVMSlept, NULL);
	/* Unregister it anyway even it wasn't registered for initializeJFR(vm, TRUE). */
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_INITIALIZED, jfrVMInitialized, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_WAITED, jfrVMMonitorWaited, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, jfrVMMonitorEntered, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_UNPARKED, jfrVMThreadParked, NULL);

	/* Free global data */
	VM_JFRConstantPoolTypes::freeJFRConstantEvents(vm);

	j9mem_free_memory((void*)vm->jfrBuffer.bufferStart);
	memset(&vm->jfrBuffer, 0, sizeof(vm->jfrBuffer));
	if (NULL != vm->jfrBufferMutex) {
		omrthread_monitor_destroy(vm->jfrBufferMutex);
		vm->jfrBufferMutex = NULL;
	}
	if (NULL != vm->jfrState.isConstantEventsInitializedMutex) {
		omrthread_monitor_destroy(vm->jfrState.isConstantEventsInitializedMutex);
		vm->jfrState.isConstantEventsInitializedMutex = NULL;
	}
	j9mem_free_memory(vm->jfrState.metaDataBlobFile);
	vm->jfrState.metaDataBlobFile = NULL;
	vm->jfrState.metaDataBlobFileSize = 0;
	if (vm->jfrAsyncKey >= 0) {
		J9UnregisterAsyncEvent(vm, vm->jfrAsyncKey);
		vm->jfrAsyncKey = -1;
	}
	if (vm->jfrThreadCPULoadAsyncKey >= 0) {
		J9UnregisterAsyncEvent(vm, vm->jfrThreadCPULoadAsyncKey);
		vm->jfrThreadCPULoadAsyncKey = -1;
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
	event->startTicks = j9time_nano_time();
	event->eventType = eventType;
	event->vmThread = currentThread;
}

jboolean
isJFREnabled(J9JavaVM *vm)
{
	return J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_JFR_ENABLED) ? JNI_TRUE : JNI_FALSE;
}

jboolean
isJFRRecordingStarted(J9JavaVM *vm)
{
	return vm->jfrState.isStarted ? JNI_TRUE : JNI_FALSE;
}

// TODO: add thread state parameter
void
jfrExecutionSample(J9VMThread *currentThread, J9VMThread *sampleThread)
{
#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! execution sample %p %p\n", currentThread, sampleThread);
#endif /* defined(DEBUG) */

	J9JFRExecutionSample *jfrEvent = (J9JFRExecutionSample*)reserveBufferWithStackTrace(currentThread, sampleThread, J9JFR_EVENT_TYPE_EXECUTION_SAMPLE, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		jfrEvent->threadState = J9JFR_THREAD_STATE_RUNNING;
	}
}

static void
jfrExecutionSampleCallback(J9VMThread *currentThread, IDATA handlerKey, void *userData)
{
	jfrExecutionSample(currentThread, currentThread);
}

static void
jfrCPULoad(J9VMThread *currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	omrthread_process_time_t currentProcCPUTimes = {0};
	intptr_t processTimeRC = omrthread_get_process_times(&currentProcCPUTimes);

	J9SysinfoCPUTime currentSysCPUTime = {0};
	intptr_t sysTimeRC = omrsysinfo_get_CPU_utilization(&currentSysCPUTime);

	if ((0 == processTimeRC) && (0 == sysTimeRC)) {
		J9JFRCPULoad *jfrEvent = (J9JFRCPULoad *)reserveBuffer(currentThread, sizeof(J9JFRCPULoad));
		if (NULL != jfrEvent) {
			initializeEventFields(currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_CPU_LOAD);

			JFRState *jfrState = &currentThread->javaVM->jfrState;
			uintptr_t numberOfCpus = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_ONLINE);
			int64_t currentTime = j9time_nano_time();

			if (-1 == jfrState->prevProcTimestamp) {
				jfrEvent->jvmUser = 0;
				jfrEvent->jvmSystem = 0;
			} else {
				int64_t timeDelta = currentTime - jfrState->prevProcTimestamp;
				float timeElapsedAllCores = (float)numberOfCpus * timeDelta;
				float userTime = (currentProcCPUTimes._userTime - jfrState->prevProcCPUTimes._userTime) / timeElapsedAllCores;
				float systemTime = (currentProcCPUTimes._systemTime - jfrState->prevProcCPUTimes._systemTime) / timeElapsedAllCores;

				jfrEvent->jvmUser = OMR_MIN(userTime, 1.0f);
				jfrEvent->jvmSystem = OMR_MIN(systemTime, 1.0f);
			}
			jfrState->prevProcCPUTimes = currentProcCPUTimes;
			jfrState->prevProcTimestamp = currentTime;

			if (-1 == jfrState->prevSysCPUTime.timestamp) {
				jfrEvent->machineTotal = 0;
			} else {
				float machineTotal =
						(currentSysCPUTime.cpuTime - jfrState->prevSysCPUTime.cpuTime)
						/ ((float)numberOfCpus
						* (currentSysCPUTime.timestamp - jfrState->prevSysCPUTime.timestamp));
				jfrEvent->machineTotal = OMR_MIN(machineTotal, 1.0f);
			}
			jfrState->prevSysCPUTime = currentSysCPUTime;
		}
	}
}

static void
jfrThreadCPULoad(J9VMThread *currentThread, J9VMThread *sampleThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);

	omrthread_thread_time_t threadTimes;
	memset(&threadTimes, 0, sizeof(threadTimes));

	intptr_t rc = omrthread_get_thread_times(&threadTimes);

	if (-1 != rc) {
		J9JFRThreadCPULoad *jfrEvent = (J9JFRThreadCPULoad *)reserveBuffer(currentThread, sizeof(J9JFRThreadCPULoad));
		if (NULL != jfrEvent) {
			initializeEventFields(currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_THREAD_CPU_LOAD);

			J9ThreadJFRState *jfrState = &sampleThread->threadJfrState;
			int64_t currentTime = j9time_nano_time();

			if (-1 == jfrState->prevTimestamp) {
				jfrEvent->userCPULoad = 0;
				jfrEvent->systemCPULoad = 0;
			} else {
				float timeDelta = (float)(currentTime - jfrState->prevTimestamp);
				float userCPULoad = (threadTimes.userTime - jfrState->prevThreadCPUTimes.userTime) / timeDelta;
				float systemCPULoad = (threadTimes.sysTime - jfrState->prevThreadCPUTimes.sysTime) / timeDelta;
				jfrEvent->userCPULoad = OMR_MIN(userCPULoad, 1.0f);
				jfrEvent->systemCPULoad = OMR_MIN(systemCPULoad, 1.0f);
			}
			jfrState->prevTimestamp = currentTime;
			jfrState->prevThreadCPUTimes = threadTimes;
		}
	}
}

static void
jfrThreadCPULoadCallback(J9VMThread *currentThread, IDATA handlerKey, void *userData)
{
	jfrThreadCPULoad(currentThread, currentThread);
}

static void
jfrClassLoadingStatistics(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9JFRClassLoadingStatistics *jfrEvent = (J9JFRClassLoadingStatistics *)reserveBuffer(currentThread, sizeof(J9JFRClassLoadingStatistics));

	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_CLASS_LOADING_STATISTICS);

		UDATA unloadedClassCount = 0;
		UDATA unloadedAnonClassCount = 0;
		vm->memoryManagerFunctions->j9gc_get_cumulative_class_unloading_stats(currentThread, &unloadedAnonClassCount, &unloadedClassCount, NULL);
		jfrEvent->unloadedClassCount = (I_64)(unloadedClassCount + unloadedAnonClassCount);
		jfrEvent->loadedClassCount = vm->loadedClassCount;
	}
}

static void
jfrThreadContextSwitchRate(J9VMThread *currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	uint64_t switches = 0;
	int32_t rc = omrsysinfo_get_number_context_switches(&switches);

	if (0 == rc) {
		J9JFRThreadContextSwitchRate *jfrEvent = (J9JFRThreadContextSwitchRate *)reserveBuffer(currentThread, sizeof(J9JFRThreadContextSwitchRate));
		if (NULL != jfrEvent) {
			JFRState *jfrState = &currentThread->javaVM->jfrState;
			int64_t currentTime = j9time_nano_time();

			initializeEventFields(currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_THREAD_CONTEXT_SWITCH_RATE);

			if ((-1 == jfrState->prevContextSwitchTimestamp)
				|| (currentTime == jfrState->prevContextSwitchTimestamp)
			) {
				jfrEvent->switchRate = 0.0f;
			} else {
				int64_t timeDelta = currentTime - jfrState->prevContextSwitchTimestamp;
				jfrEvent->switchRate = (switches - jfrState->prevContextSwitches) * 1e9f / timeDelta;
			}
			jfrState->prevContextSwitches = switches;
			jfrState->prevContextSwitchTimestamp = currentTime;
		}
	}
}

static void
jfrThreadStatistics(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9JFRThreadStatistics *jfrEvent = (J9JFRThreadStatistics *)reserveBuffer(currentThread, sizeof(J9JFRThreadStatistics));

	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_THREAD_STATISTICS);

		jfrEvent->activeThreadCount = vm->totalThreadCount;
		jfrEvent->daemonThreadCount = vm->daemonThreadCount;
		jfrEvent->accumulatedThreadCount = vm->accumulatedThreadCount;
		jfrEvent->peakThreadCount = vm->peakThreadCount;
	}

}

static int J9THREAD_PROC
jfrSamplingThreadProc(void *entryArg)
{
	J9JavaVM *vm = (J9JavaVM*)entryArg;
	J9VMThread *currentThread = NULL;

	if (JNI_OK == attachSystemDaemonThread(vm, &currentThread, "JFR sampler")) {
		omrthread_monitor_enter(vm->jfrSamplerMutex);
		vm->jfrSamplerState = J9JFR_SAMPLER_STATE_RUNNING;
		omrthread_monitor_notify_all(vm->jfrSamplerMutex);
		UDATA count = 0;
		while (J9JFR_SAMPLER_STATE_STOP != vm->jfrSamplerState) {
			J9SignalAsyncEvent(vm, NULL, vm->jfrAsyncKey);
			if (0 == (count % 100)) { // 1 second
				omrthread_monitor_exit(vm->jfrSamplerMutex);
				internalAcquireVMAccess(currentThread);
				jfrCPULoad(currentThread);
				jfrClassLoadingStatistics(currentThread);
				jfrThreadStatistics(currentThread);
				if (0 == (count % 1000)) { // 10 seconds
					J9SignalAsyncEvent(vm, NULL, vm->jfrThreadCPULoadAsyncKey);
					jfrThreadContextSwitchRate(currentThread);
				}
				internalReleaseVMAccess(currentThread);
				omrthread_monitor_enter(vm->jfrSamplerMutex);
			}
			count += 1;
			omrthread_monitor_wait_timed(vm->jfrSamplerMutex, J9JFR_SAMPLING_RATE, 0);
		}
		omrthread_monitor_exit(vm->jfrSamplerMutex);
		DetachCurrentThread((JavaVM*)vm);
	}

	omrthread_monitor_enter(vm->jfrSamplerMutex);
	vm->jfrSamplerState = J9JFR_SAMPLER_STATE_DEAD;
	omrthread_monitor_notify_all(vm->jfrSamplerMutex);
	omrthread_exit(vm->jfrSamplerMutex);
	return 0;
}

jboolean
setJFRRecordingFileName(J9JavaVM *vm, char *newFileName)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	VM_JFRWriter::closeJFRFile(vm);
	j9mem_free_memory(vm->jfrState.jfrFileName);
	vm->jfrState.jfrFileName = newFileName;
	return VM_JFRWriter::openJFRFile(vm) ? JNI_TRUE : JNI_FALSE;
}

/**
 * Must be holding on to exclusive VM access.
 */
void
jfrDump(J9VMThread *currentThread, BOOLEAN finalWrite)
{
	/* Flush all the thread buffers and write out the global buffer. */
	flushAllThreadBuffers(currentThread, finalWrite);
	writeOutGlobalBuffer(currentThread, finalWrite, true);
}

static UDATA
jfrTypeIDHashFn(void *key, void *userData)
{
	const J9JFRTypeID *entry = (const J9JFRTypeID *)key;

	return (UDATA)entry->clazz;
}

static UDATA
jfrTypeIDHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	const J9JFRTypeID *tableEntry = (const J9JFRTypeID *)tableNode;
	const J9JFRTypeID *queryEntry = (const J9JFRTypeID *)queryNode;

	return tableEntry->clazz == queryEntry->clazz;
}

UDATA
initializeJFRIDs(J9JavaVM *vm)
{
	UDATA result = 0;

	if (omrthread_monitor_init_with_name(&vm->jfrState.typeIDMonitor, 0, "JFR Type ID monitor")) {
		result = 1;
		goto done;
	}

	vm->jfrState.typeIDcount = RESERVED_CLASS_ID_LIMIT;

done:
	return result;
}

void
shutdownJFRIDs(J9JavaVM *vm)
{
	if (NULL != vm->jfrState.typeIDMonitor) {
		omrthread_monitor_destroy(vm->jfrState.typeIDMonitor);
		vm->jfrState.typeIDMonitor = NULL;
	}
}

/**
 * Known event types are only ever looked up by name and are always
 * JVM event types so they can be below the RESERVED_CLASS_ID_LIMIT. The
 * JVM determines the IDs for these types. They will always be loaded
 * by the boot loader.
 *
 * TODO this function will be expanded in the future to provide general
 * support for known JVM event types. Currently, the types supported are
 * the ones needed to bootstrap JFR JCL initialization.
 */
static jlong
getKnownJFREventType(const J9UTF8 *className)
{
	jlong result = INVALID_TYPE_ID;
	if (J9UTF8_LITERAL_EQUALS_UTF8(className, "boolean")) {
		result = BOOLEAN_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "byte")) {
		result = BYTE_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "char")) {
		result = CHAR_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "short")) {
		result = SHORT_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "int")) {
		result = INT_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "float")) {
		result = FLOAT_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "long")) {
		result = LONG_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "double")) {
		result = DOUBLE_TYPE_ID;
	} else if (J9UTF8_LITERAL_EQUALS_UTF8(className, "jdk/types/StackTrace")) {
		result = STACKTRACE_TYPE_ID;
	}

	return result;
}

jlong
getTypeIdUTF8(J9VMThread *currentThread, const J9UTF8 *className)
{
	J9JavaVM *vm = currentThread->javaVM;
	jlong result = INVALID_TYPE_ID;

	Trc_VM_getTypeIdUTF8_Entry(currentThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className));

	omrthread_monitor_enter(vm->classTableMutex);
	J9Class *clazz = hashClassTableAt(vm->systemClassLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
	omrthread_monitor_exit(vm->classTableMutex);

	if (NULL != clazz) {
		result = getTypeId(currentThread, clazz);
	} else {
		result = getKnownJFREventType(className);
	}

	Trc_VM_getTypeIdUTF8_Exit(currentThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), clazz, result);

	return result;
}

jlong
getTypeId(J9VMThread *currentThread, J9Class *clazz)
{
	J9JavaVM *vm = currentThread->javaVM;
	jlong result = INVALID_TYPE_ID;
	J9JFRTypeID entry = {0};
	J9JFRTypeID *jfrTypeID = &entry;

	Trc_VM_getTypeId_Entry(currentThread, clazz);

	Assert_VM_mustHaveVMAccess(currentThread);

	omrthread_monitor_enter(vm->jfrState.typeIDMonitor);

	J9HashTable *typeIDTable = clazz->classLoader->typeIDs;

	if (NULL == typeIDTable) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		typeIDTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary),
								J9_GET_CALLSITE(),
								0,
								sizeof(J9JFRTypeID),
								sizeof(J9JFRTypeID *),
								0,
								J9MEM_CATEGORY_CLASSES,
								jfrTypeIDHashFn,
								jfrTypeIDHashEqualFn,
								NULL,
								NULL);

		if (NULL == typeIDTable) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
		clazz->classLoader->typeIDs = typeIDTable;
	}

	jfrTypeID->clazz = clazz;
	jfrTypeID = (J9JFRTypeID *)hashTableFind(typeIDTable, jfrTypeID);

	if (NULL == jfrTypeID) {
		jfrTypeID = &entry;
		jfrTypeID->id = vm->jfrState.typeIDcount;

		vm->jfrState.typeIDcount += 1;
		Assert_VM_true(vm->jfrState.typeIDcount > 0);

		jfrTypeID = (J9JFRTypeID *)hashTableAdd(typeIDTable, jfrTypeID);
		if (NULL == jfrTypeID) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	}
	result = jfrTypeID->id;

done:
	omrthread_monitor_exit(vm->jfrState.typeIDMonitor);
	Trc_VM_getTypeId_Exit(currentThread, clazz, result);

	return result;
}

} /* extern "C" */

#endif /* defined(J9VM_OPT_JFR) */
