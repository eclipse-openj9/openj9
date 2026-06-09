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
#include "objhelp.h"
#include "pool_api.h"
#include "thread_api.h"
#include "ut_j9vm.h"
#include "vm_internal.h"

#if defined(J9VM_OPT_JFR)

#include "AtomicSupport.hpp"
#if JAVA_SPEC_VERSION >= 17
#include "JFRTypeMappings.hpp"
#include "JFRPeriodic.hpp"
#endif /* JAVA_SPEC_VERSION >= 17 */
#include "JFRWriter.hpp"

extern "C" {

#undef DEBUG

J9_DECLARE_CONSTANT_UTF8(initJFRUTF8, "initJFRv2");
J9_DECLARE_CONSTANT_UTF8(initJFRSigUTF8, "()V");
J9_DECLARE_CONSTANT_NAS(initJFRNAS, initJFRUTF8, initJFRSigUTF8);
J9_DECLARE_CONSTANT_UTF8(jfrHelpersUTF8, "java/lang/JFRHelpers");
J9_DECLARE_CONSTANT_UTF8(bytesForEagerInstrumentationUTF8, "transformClassAndInvokebytesForEagerInstrumentation");
J9_DECLARE_CONSTANT_UTF8(bytesForEagerInstrumentationSigUTF8, "(JZLjava/lang/Class;[BZ)[B");
J9_DECLARE_CONSTANT_NAS(bytesForEagerInstrumentationNAS, bytesForEagerInstrumentationUTF8, bytesForEagerInstrumentationSigUTF8);
J9_DECLARE_CONSTANT_UTF8(transformToListUTF8, "transformToList");
J9_DECLARE_CONSTANT_UTF8(transformToListSigUTF8, "([Ljava/lang/Object;)Ljava/util/List;");
J9_DECLARE_CONSTANT_NAS(transformToListNAS, transformToListUTF8, transformToListSigUTF8);
J9_DECLARE_CONSTANT_UTF8(jfrInternalEventClassUTF8, "jdk/internal/event/Event");
J9_DECLARE_CONSTANT_UTF8(jfrEventClassUTF8, "jdk/jfr/Event");

// TODO: allow configureable values
#define J9JFR_THREAD_BUFFER_SIZE (128 * 1024)
#define J9JFR_GLOBAL_BUFFER_SIZE (10 * J9JFR_THREAD_BUFFER_SIZE)
#define J9JFR_SAMPLING_RATE 10
#define J9JFR_CLASSNAME_BUFFER_SIZE 128

#define INVALID_TYPE_ID -1

static void jfrStartSamplingThread(J9JavaVM *vm);
static void initializeEventFields(J9VMThread *currentThread, J9VMThread *sampleThread, J9JFREvent *jfrEvent, UDATA eventType);
static I_64 getThreadTID(J9VMThread *currentThread, J9VMThread *vmThread);
static int J9THREAD_PROC jfrSamplingThreadProc(void *entryArg);
static void jfrExecutionSampleCallback(J9VMThread *currentThread, IDATA handlerKey, void *userData);
static void jfrThreadCPULoadCallback(J9VMThread *currentThread, IDATA handlerKey, void *userData);
static void jfrCheckJFRCMDLineOptions(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static jlong getTypeIdImpl(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *className, BOOLEAN freeName, J9Class *clazz = NULL);
#if JAVA_SPEC_VERSION >= 17
static bool addEventIds(J9JavaVM *vm);
static bool addTypeIds(J9JavaVM *vm);
#endif /* JAVA_SPEC_VERSION >= 17 */
static void jfrShutdownInternalStructures(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);
static void notifyForChunkRotation(J9VMThread *currentThread);
static void checkAvailableSpaceInGlobalBuffer(J9VMThread *currentThread);
static void jfrClassInitialize(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData);

static void
freeThreadIDs(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	J9HashTableState hashTableState = {0};
	J9JFRThreadObject *entry = (J9JFRThreadObject *)hashTableStartDo(vm->jfrState.threadObjectJNIRefTable, &hashTableState);

	while (NULL != entry) {
		vmFuncs->j9jni_deleteGlobalRef((JNIEnv *)currentThread, entry->threadObject, false);
		entry = (J9JFRThreadObject *)hashTableNextDo(&hashTableState);
	}

	hashTableFree(vm->jfrState.threadObjectJNIRefTable);
	vm->jfrState.threadObjectJNIRefTable = NULL;
}

static UDATA
jfrThreadObjectHashFn(void *key, void *userData)
{
	J9JFRThreadObject *entry = (J9JFRThreadObject *)key;

	return (UDATA)entry->javaTID;
}

static UDATA
jfrThreadObjectEqualFn(void *tableNode, void *queryNode, void *userData)
{
	J9JFRThreadObject *tableEntry = (J9JFRThreadObject *)tableNode;
	J9JFRThreadObject *queryEntry = (J9JFRThreadObject *)queryNode;

	return tableEntry->javaTID == queryEntry->javaTID;
}

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
		size = sizeof(J9JFRExecutionSample) + (((J9JFRExecutionSample *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_THREAD_START:
		size = sizeof(J9JFRThreadStart) + (((J9JFRThreadStart *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_THREAD_END:
		size = sizeof(J9JFREvent);
		break;
	case J9JFR_EVENT_TYPE_THREAD_SLEEP:
		size = sizeof(J9JFRThreadSlept) + (((J9JFRThreadSlept *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_OBJECT_WAIT:
		size = sizeof(J9JFRMonitorWaited) + (((J9JFRMonitorWaited *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_MONITOR_ENTER:
		size = sizeof(J9JFRMonitorEntered) + (((J9JFRMonitorEntered *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_THREAD_PARK:
		size = sizeof(J9JFRThreadParked) + (((J9JFRThreadParked *)jfrEvent)->stackTraceSize * sizeof(UDATA));
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
	case J9JFR_EVENT_TYPE_SYSTEM_GC:
		size = sizeof(J9JFRSystemGC) + (((J9JFRSystemGC *)jfrEvent)->stackTraceSize * sizeof(UDATA));
		break;
	case J9JFR_EVENT_TYPE_OLD_GC_ENTRY:
		size = sizeof(J9JFROldGarbageCollection);
		break;
	case J9JFR_EVENT_TYPE_YOUNG_GC_ENTRY:
		size = sizeof(J9JFRYoungGarbageCollection);
		break;
	case J9JFR_EVENT_TYPE_GARBAGE_COLLECTION_ENTRY:
		size = sizeof(J9JFRGarbageCollection);
		break;
	case J9JFR_EVENT_TYPE_GC_HEAP_SUMMARY_ENTRY:
		size = sizeof(J9JFRGCHeapSummary);
		break;
	case J9JFR_EVENT_TYPE_NETWORKUTILIZATION:
		size = sizeof(J9JFRNetworkUtilization);
		break;
	case J9JFR_EVENT_TYPE_DATA_LOSS:
		size = sizeof(J9JFRDataLoss);
		break;
	case J9JFR_EVENT_TYPE_JVM_INFORMATION:
	case J9JFR_EVENT_TYPE_CPU_INFORMATION:
	case J9JFR_EVENT_TYPE_VIRTUALIZATION_INFORMATION:
	case J9JFR_EVENT_TYPE_OS_INFORMATION:
	case J9JFR_EVENT_TYPE_INITIAL_SYSTEM_PROPERTY:
	case J9JFR_EVENT_TYPE_INITIAL_ENVIRONMENT_VARIABLE:
	case J9JFR_EVENT_TYPE_GC_HEAP_CONFIGURATION:
	case J9JFR_EVENT_TYPE_YOUNG_GENERATION_CONFIGURATION:
	case J9JFR_EVENT_TYPE_PHYSICAL_MEMORY:
	case J9JFR_EVENT_TYPE_SYSTEM_PROCESS:
	case J9JFR_EVENT_TYPE_MODULE_REQUIRE:
	case J9JFR_EVENT_TYPE_MODULE_EXPORT:
	case J9JFR_EVENT_TYPE_CLASS_LOADER_STATISTICS:
	case J9JFR_EVENT_TYPE_NATIVE_LIBRARY:
	case J9JFR_EVENT_TYPE_THREAD_DUMP:
		size = sizeof(J9JFREvent);
		break;
	default:
		Assert_VM_unreachable();
		break;
	}
	return size;
}

J9JFREvent *
jfrBufferStartDo(J9JFRBuffer *buffer, J9JFRBufferWalkState *walkState)
{
	U_8 *start = buffer->bufferStart;
	U_8 *end = buffer->bufferCurrent;
	walkState->end = end;
	walkState->current = start;
	if (start == end) {
		start = NULL;
	}
	return (J9JFREvent *)start;
}

J9JFREvent *
jfrBufferNextDo(J9JFRBufferWalkState *walkState)
{
	U_8 *current = walkState->current;
	U_8 *next = current + jfrEventSize((J9JFREvent *)current);

	Assert_VM_true(walkState->end >= next);

	if (walkState->end == next) {
		next = NULL;
	}

	walkState->current = (U_8 *)next;

	return (J9JFREvent *)next;
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

#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! flushing %p of size %u start=%p current=%p\n", flushThread, (U_32)bufferSize, flushThread->jfrBuffer.bufferStart, flushThread->jfrBuffer.bufferCurrent);
#endif /* defined(DEBUG) */

	if (!areJFRBuffersReadyForWrite(flushThread)) {
		goto done;
	}

	omrthread_monitor_enter(vm->jfrBufferMutex);
	if (vm->jfrBuffer.bufferRemaining < bufferSize) {
		if (isJFRV2SupportEnabled(vm)) {
			notifyForChunkRotation(currentThread);
			omrthread_monitor_exit(vm->jfrBufferMutex);
			success = false;
			goto done;
		} else if (!writeOutGlobalBuffer(currentThread, false, false)) {
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
	checkAvailableSpaceInGlobalBuffer(currentThread);

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
			j9mem_free_memory((void *)loopThread->jfrBuffer.bufferStart);
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
			j9mem_free_memory((void *)currentThread->jfrBuffer.bufferStart);
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
U_8 *
reserveBuffer(J9VMThread *currentThread, J9VMThread *sampleThread, UDATA size)
{
	J9JavaVM *vm = currentThread->javaVM;
	U_8 *jfrEvent = NULL;

	/* Either we are holding on to VM access or this operation is driven by another thread that has exclusive. */
	Assert_VM_mustHaveVMAccess(currentThread);
	Assert_VM_true((currentThread != sampleThread) ? ((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState)) : TRUE);

	if (!areJFRBuffersReadyForWrite(sampleThread)
		|| ((NULL != sampleThread->threadObject)
			&& isJFRRecordingDisabledOnThread(currentThread, sampleThread->threadObject))
	) {
		goto done;
	}

	/* If the event is larger than the buffer, fail without attemptiong to flush */
	if (size <= sampleThread->jfrBuffer.bufferSize) {
		/* If there isn't enough space, flush the thread buffer to global */
		if (size > sampleThread->jfrBuffer.bufferRemaining) {
			if (!flushBufferToGlobal(currentThread, sampleThread)) {
				goto done;
			}
		}
		jfrEvent = sampleThread->jfrBuffer.bufferCurrent;
		sampleThread->jfrBuffer.bufferCurrent += size;
		sampleThread->jfrBuffer.bufferRemaining -= size;
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
static J9JFREvent *
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
		jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, sampleThread, eventSize);
		if (NULL != jfrEvent) {
			Assert_VM_mustHaveVMAccess(currentThread);
			initializeEventFields(currentThread, sampleThread, jfrEvent, eventType);
			((J9JFREventWithStackTrace *)jfrEvent)->stackTraceSize = framesWalked;
			memcpy(((U_8 *)jfrEvent) + eventFixedSize, walkState->cache, stackTraceBytes);
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
	j9tty_printf(PORTLIB, "\n!!! thread created %p\n", currentThread);
#endif /* defined(DEBUG) */

	/* TODO: allow different buffer sizes on different threads. */
	U_8 *buffer = (U_8 *)j9mem_allocate_memory(J9JFR_THREAD_BUFFER_SIZE, J9MEM_CATEGORY_JFR);
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

	/* Skip recording the ThreadStart event if the started thread is JFR-excluded.
	 * reserveBuffer checks the sample thread (currentThread here), not the started thread.
	 */
	if ((NULL != startedThread->threadObject)
		&& isJFRRecordingDisabledOnThread(currentThread, startedThread->threadObject)
	) {
		return;
	}

	J9JFRThreadStart *jfrEvent = (J9JFRThreadStart *)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_THREAD_START, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		jfrEvent->threadTID = getThreadTID(currentThread, startedThread);
		jfrEvent->parentThreadTID = getThreadTID(currentThread, currentThread);
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
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! thread end %p\n", currentThread);
#endif /* defined(DEBUG) */

	internalAcquireVMAccess(currentThread);
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_THREAD_END);
	}
	flushBufferToGlobal(currentThread, currentThread);

	/* Free the thread local buffer */
	j9mem_free_memory((void *)currentThread->jfrBuffer.bufferStart);
	memset(&currentThread->jfrBuffer, 0, sizeof(currentThread->jfrBuffer));
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

	J9JFRThreadSlept *jfrEvent = (J9JFRThreadSlept *)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_THREAD_SLEEP, sizeof(*jfrEvent));
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
	IDATA rc = omrthread_create(&(vm->jfrSamplerThread), vm->defaultOSStackSize, J9THREAD_PRIORITY_NORMAL, FALSE, jfrSamplingThreadProc, (void *)vm);
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
	J9VMMonitorWaitedEvent *event = (J9VMMonitorWaitedEvent *) eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! VM monitor waited %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFRMonitorWaited *jfrEvent = (J9JFRMonitorWaited *)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_OBJECT_WAIT, sizeof(*jfrEvent));
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
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_MONITOR_ENTER);

		jfrEvent->duration = j9time_nano_time() - event->startTicks;
		jfrEvent->monitorClass = event->monitorClass;
		jfrEvent->monitorAddress = (UDATA)event->monitor;
		jfrEvent->previousOwnerTID = getThreadTID(currentThread, event->previousOwner);
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
jfrVMThreadParked(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMUnparkedEvent *event = (J9VMUnparkedEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

#if defined(DEBUG)
	j9tty_printf(PORTLIB, "\n!!! thread park %p\n", currentThread);
#endif /* defined(DEBUG) */

	J9JFRThreadParked *jfrEvent = (J9JFRThreadParked *)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_THREAD_PARK, sizeof(*jfrEvent));
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

/**
 * Hook for system GC. Called without VM access.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
static void
jfrSystemGC(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMSystemGCEvent *event = (J9VMSystemGCEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	PORT_ACCESS_FROM_VMC(currentThread);

	J9JFRSystemGC *jfrEvent = (J9JFRSystemGC *)reserveBufferWithStackTrace(currentThread, currentThread, J9JFR_EVENT_TYPE_SYSTEM_GC, sizeof(*jfrEvent));
	if (NULL != jfrEvent) {
		jfrEvent->startTicks = event->startTicks;
		jfrEvent->duration = j9time_nano_time() - event->startTicks;
	}
}

/**
 * Hook for old garbage collection event. Called without VM access.
 *
 * @param omrVMThread the omr VM thread
 */
void
jfrOldGarbageCollection(OMR_VMThread *omrVMThread)
{
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;

	J9JFROldGarbageCollection *jfrEvent = (J9JFROldGarbageCollection *)reserveBuffer(currentThread, currentThread, sizeof(J9JFROldGarbageCollection));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_OLD_GC_ENTRY);
		jfrEvent->startTicks = javaVM->memoryManagerFunctions->j9gc_get_cycle_start_time(currentThread);
		jfrEvent->duration = javaVM->memoryManagerFunctions->j9gc_get_cycle_end_time(currentThread) - jfrEvent->startTicks;
		jfrEvent->gcID = javaVM->memoryManagerFunctions->j9gc_get_unique_cycle_ID(currentThread);
	}
}

/**
 * Hook for young garbage collection event. Called without VM access.
 *
 * @param omrVMThread the omr VM thread
 */
void
jfrYoungGarbageCollection(OMR_VMThread *omrVMThread)
{
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;

	J9JFRYoungGarbageCollection *jfrEvent = (J9JFRYoungGarbageCollection *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRYoungGarbageCollection));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_YOUNG_GC_ENTRY);
		jfrEvent->startTicks = javaVM->memoryManagerFunctions->j9gc_get_cycle_start_time(currentThread);
		jfrEvent->duration = javaVM->memoryManagerFunctions->j9gc_get_cycle_end_time(currentThread) - jfrEvent->startTicks;
		jfrEvent->gcID = javaVM->memoryManagerFunctions->j9gc_get_unique_cycle_ID(currentThread);
		jfrEvent->tenureThreshold = javaVM->memoryManagerFunctions->j9gc_get_tenure_threshold(javaVM);
	}
}

/**
 * Hook for garbage collection event. Called without VM access.
 *
 * @param omrVMThread the omr VM thread
 */
void
jfrGarbageCollection(OMR_VMThread *omrVMThread)
{
	/* Extract the J9VMThread from the OMR_VMThread */
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;

	/* Reserve buffer space for the JFR event */
	J9JFRGarbageCollection *jfrEvent = (J9JFRGarbageCollection *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRGarbageCollection));
	if (NULL != jfrEvent) {
		/* Initialize common event fields (timestamp, thread ID, etc.) */
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_GARBAGE_COLLECTION_ENTRY);

		/* Populate GC-specific fields using memory manager functions */
		jfrEvent->startTicks = javaVM->memoryManagerFunctions->j9gc_get_cycle_start_time(currentThread);
		jfrEvent->duration = javaVM->memoryManagerFunctions->j9gc_get_cycle_end_time(currentThread) - jfrEvent->startTicks;
		jfrEvent->gcID = javaVM->memoryManagerFunctions->j9gc_get_unique_cycle_ID(currentThread);
		jfrEvent->gcNameID = javaVM->memoryManagerFunctions->j9gc_get_gc_collector_type(currentThread);
		jfrEvent->gcCauseID = javaVM->memoryManagerFunctions->j9gc_get_gc_cause_type(currentThread);
		jfrEvent->sumOfPauses = javaVM->memoryManagerFunctions->j9gc_get_sum_of_pauses(currentThread);
		jfrEvent->longestPause = javaVM->memoryManagerFunctions->j9gc_get_longest_pause(currentThread);
	}
}

void
jfrGCHeapSummary(OMR_VMThread *omrVMThread, U_32 gcWhenID)
{
	/* Extract the J9VMThread from the OMR_VMThread */
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;

	/* Reserve buffer space for the JFR event */
	J9JFRGCHeapSummary *jfrEvent = (J9JFRGCHeapSummary *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRGCHeapSummary));
	if (NULL != jfrEvent) {
		/* Initialize common event fields (timestamp, thread ID, etc.) */
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_GC_HEAP_SUMMARY_ENTRY);

		/* Populate GC heap summary fields */
		jfrEvent->startTicks = javaVM->memoryManagerFunctions->j9gc_get_cycle_start_time(currentThread);
		jfrEvent->duration = 0; /* Duration is 0 for heap summary events */
		jfrEvent->gcID = javaVM->memoryManagerFunctions->j9gc_get_unique_cycle_ID(currentThread);
		jfrEvent->gcWhenID = gcWhenID; /* BeforeGC or AfterGC */

		/* Get heap information from memory manager */
		void *heapBase = javaVM->memoryManagerFunctions->j9gc_get_heap_base(javaVM);
		void *heapTop = javaVM->memoryManagerFunctions->j9gc_get_heap_top(javaVM);
		UDATA heapSize = javaVM->memoryManagerFunctions->j9gc_heap_total_memory(javaVM);
		UDATA freeMemory = javaVM->memoryManagerFunctions->j9gc_heap_free_memory(javaVM);

		/* Populate virtual space fields */
		jfrEvent->vStart = (U_64)(uintptr_t)heapBase;
		jfrEvent->vCommittedEnd = (U_64)((uintptr_t)heapBase + heapSize);
		jfrEvent->vCommittedSize = (U_64)heapSize;
		jfrEvent->vReservedEnd = (U_64)(uintptr_t)heapTop;
		jfrEvent->vReservedSize = (U_64)((uintptr_t)heapTop - (uintptr_t)heapBase);

		/* Calculate heap used */
		jfrEvent->heapUsed = (I_64)(heapSize - freeMemory);
	}
}

jint
initializeJFR(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);
	jint rc = JNI_ERR;

	U_8 *buffer = NULL;
	UDATA timeSuccess = 0;
	J9VMThread *walkThread = NULL;

	Assert_VM_false(vm->jfrState.isCreated);

	/* Register async handler for execution samples. */
	vm->jfrAsyncKey = J9RegisterAsyncEvent(vm, jfrExecutionSampleCallback, NULL);
	if (vm->jfrAsyncKey < 0) {
		goto fail;
	}

	vm->jfrThreadCPULoadAsyncKey = J9RegisterAsyncEvent(vm, jfrThreadCPULoadCallback, NULL);
	if (vm->jfrThreadCPULoadAsyncKey < 0) {
		goto fail;
	}

	/* Allocate constantEvents. */
	vm->jfrState.constantEvents = j9mem_allocate_memory(sizeof(JFRConstantEvents), J9MEM_CATEGORY_JFR);
	if (NULL == vm->jfrState.constantEvents) {
		goto fail;
	}
	memset(vm->jfrState.constantEvents, 0, sizeof(JFRConstantEvents));

	/* Allocate global data. */
	buffer = (U_8 *)j9mem_allocate_memory(J9JFR_GLOBAL_BUFFER_SIZE, J9MEM_CATEGORY_JFR);
	if (NULL == buffer) {
		goto fail;
	}

	vm->jfrState.threadObjectJNIRefTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary), J9_GET_CALLSITE(), 0, sizeof(J9JFRThreadObject), sizeof(U_64), 0, J9MEM_CATEGORY_JFR, jfrThreadObjectHashFn, jfrThreadObjectEqualFn, NULL, NULL);
	if (NULL == vm->jfrState.threadObjectJNIRefTable) {
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

	vm->jfrState.prevProcTimestamp = -1;

	if (0 == omrsysinfo_get_number_context_switches(&vm->jfrState.prevContextSwitches)) {
		vm->jfrState.prevContextSwitchTimestamp = j9time_nano_time();
	} else {
		vm->jfrState.prevContextSwitchTimestamp = -1;
		vm->jfrState.prevContextSwitches = 0;
	}

	/* Initialize network interface stats linked list. */
	vm->jfrState.prevNetworkTimestamp = -1;
	vm->jfrState.networkInterfaceStatsList = NULL;

	if (omrthread_monitor_init_with_name(&vm->jfrBufferMutex, 0, "JFR global buffer mutex")) {
		goto fail;
	}
	if (omrthread_monitor_init_with_name(&vm->jfrSamplerMutex, 0, "JFR sampler mutex")) {
		goto fail;
	}
	if (omrthread_monitor_init_with_name(&vm->jfrState.isConstantEventsInitializedMutex, 0, "Is JFR constantEvents initialized mutex")) {
		goto fail;
	}
	if (omrthread_monitor_init_with_name(&vm->jfrState.threadObjectsMutex, 0, "Thread objects mutex")) {
		goto fail;
	}

	if (!VM_JFRWriter::initializaJFRWriter(vm)) {
		goto fail;
	}

	/* Go through existing threads. */
	walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		/* only initialize a thread once */
		if (NULL == walkThread->jfrBuffer.bufferStart) {
			U_8 *buffer = (U_8 *)j9mem_allocate_memory(J9JFR_THREAD_BUFFER_SIZE, J9MEM_CATEGORY_JFR);
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

	vm->jfrState.isCreated = TRUE;

	if (!isJFRV2SupportEnabled(vm)) {
		if (!startJFRRecording(vm)) {
			goto fail;
		}
	}

done:
	rc = JNI_OK;
	return rc;
fail:
	tearDownJFR(vm);
	goto done;
}

BOOLEAN
startJFRRecording(J9JavaVM *vm)
{
	J9HookInterface **vmHooks = getVMHookInterface(vm);
	BOOLEAN rc = FALSE;

	Assert_VM_false(vm->jfrState.isStarted);
	Assert_VM_true(vm->jfrState.isCreated);

	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jfrClassesUnload, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrVMShutdown, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_STARTING, jfrThreadStarting, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_END, jfrThreadEnd, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SLEPT, jfrVMSlept, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_WAITED, jfrVMMonitorWaited, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, jfrVMMonitorEntered, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_UNPARKED, jfrVMThreadParked, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_SYSTEM_GC_CALLED, jfrSystemGC, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}
	/* Register GC-related hooks via gc_base */
	if (0 != (vm->memoryManagerFunctions->j9gc_register_jfr_hooks(vm))) {
		goto done;
	}

	jfrStartSamplingThread(vm);

	vm->jfrState.isStarted = TRUE;

	rc = TRUE;
done:
	return rc;
}

void
stopJFRRecording(J9JavaVM *vm)
{
	J9VMThread *currentThread = currentVMThread(vm);
	J9HookInterface **vmHooks = getVMHookInterface(vm);
	Assert_VM_mustHaveVMAccess(currentThread);
	internalReleaseVMAccess(currentThread);

	Assert_VM_true(vm->jfrState.isStarted);
	vm->jfrState.isStarted = FALSE;

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
	}
	vm->jfrSamplerState = J9JFR_SAMPLER_STATE_UNINITIALIZED;

	internalAcquireVMAccess(currentThread);

	/* Unhook all the events */
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_CREATED, jfrThreadCreated, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jfrClassesUnload, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrVMShutdown, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_STARTING, jfrThreadStarting, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_THREAD_END, jfrThreadEnd, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_SLEPT, jfrVMSlept, NULL);
	/* Unregister it anyway even it wasn't registered for initializeJFR(vm). */
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_INITIALIZED, jfrVMInitialized, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_WAITED, jfrVMMonitorWaited, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_MONITOR_CONTENDED_ENTERED, jfrVMMonitorEntered, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_UNPARKED, jfrVMThreadParked, NULL);
	(*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_SYSTEM_GC_CALLED, jfrSystemGC, NULL);

	/* Deregister GC-related hooks via gc_base */
	vm->memoryManagerFunctions->j9gc_deregister_jfr_hooks(vm);
}

void
tearDownJFR(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9VMThread *currentThread = currentVMThread(vm);

	Assert_VM_mustHaveVMAccess(currentThread);
	Assert_VM_true(vm->jfrState.isCreated);

	if (!isJFRV2SupportEnabled(vm)) {
		stopJFRRecording(vm);
	}
	vm->jfrState.isCreated = FALSE;
	VM_JFRWriter::teardownJFRWriter(vm);

	freeThreadIDs(currentThread);

	/* Free global data */
	VM_JFRConstantPoolTypes::freeJFRConstantEvents(vm);

	j9mem_free_memory((void *)vm->jfrBuffer.bufferStart);
	memset(&vm->jfrBuffer, 0, sizeof(vm->jfrBuffer));
	if (NULL != vm->jfrBufferMutex) {
		omrthread_monitor_destroy(vm->jfrBufferMutex);
		vm->jfrBufferMutex = NULL;
	}
	if (NULL != vm->jfrState.isConstantEventsInitializedMutex) {
		omrthread_monitor_destroy(vm->jfrState.isConstantEventsInitializedMutex);
		vm->jfrState.isConstantEventsInitializedMutex = NULL;
	}
	/* Clean up network interface stats linked list. */
	JFRNetworkInterfaceStats *current = (JFRNetworkInterfaceStats *)vm->jfrState.networkInterfaceStatsList;
	while (NULL != current) {
		JFRNetworkInterfaceStats *next = current->next;
		j9mem_free_memory(current);
		current = next;
	}
	vm->jfrState.networkInterfaceStatsList = NULL;
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
	omrthread_monitor_destroy(vm->jfrSamplerMutex);
	vm->jfrSamplerMutex = NULL;
}

static I_64
getThreadTID(J9VMThread *currentThread, J9VMThread *vmThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	U_64 tid = 0;
	j9object_t threadObject = NULL;

	Assert_VM_mustHaveVMAccess(currentThread);

	J9JFRThreadObject threadEntryBuffer = {0};
	J9JFRThreadObject *threadEntry = &threadEntryBuffer;

	if (NULL == vmThread) {
		goto done;
	}

	if (NULL == vmThread->threadObject) {
		goto done;
	}

	threadEntry->javaTID = J9VMJAVALANGTHREAD_TID(currentThread, vmThread->threadObject);

	omrthread_monitor_enter(vm->jfrState.threadObjectsMutex);

	threadEntry = (J9JFRThreadObject *)hashTableFind(vm->jfrState.threadObjectJNIRefTable, threadEntry);

	if (NULL != threadEntry) {
		tid = threadEntry->javaTID;
		goto doneWithMutex;
	}

	threadEntry = &threadEntryBuffer;
	threadObject = vmThread->threadObject;
	if (NULL != threadObject) {
		threadEntry->threadObject = vm->internalVMFunctions->j9jni_createGlobalRef((JNIEnv *)currentThread, vmThread->threadObject, FALSE);
		threadEntry->javaTID = J9VMJAVALANGTHREAD_TID(currentThread, vmThread->threadObject);
		threadEntry->nativeTID = threadEntry->javaTID;
		/* TODO JMC has trouble distinguishing different threads with the same native TID
		 * in the same chunk which will happen if a a lof of threads are created and die
		 * very quickly.
		 * threadEntry->nativeTID = ((J9AbstractThread *)vmThread->osThread)->tid;
		 */
		threadEntry = (J9JFRThreadObject *)hashTableAdd(vm->jfrState.threadObjectJNIRefTable, threadEntry);
		tid = threadEntry->javaTID;
		if (NULL == threadEntry) {
			vm->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
		}
	}

doneWithMutex:
	omrthread_monitor_exit(vm->jfrState.threadObjectsMutex);
done:
	return tid;
}

void
initializeEventFields(J9VMThread *currentThread, J9VMThread *sampleThread, J9JFREvent *event, UDATA eventType)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	Assert_VM_mustHaveVMAccess(currentThread);
	event->startTicks = j9time_nano_time();
	event->eventType = eventType;
	event->currentThreadTID = getThreadTID(currentThread, sampleThread);
}

jboolean
isJFREnabled(J9JavaVM *vm)
{
	return J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_JFR_ENABLED) ? JNI_TRUE : JNI_FALSE;
}

jboolean
isJFRV2SupportEnabled(J9JavaVM *vm)
{
	return J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_JFR_V2_SUPPORT) ? JNI_TRUE : JNI_FALSE;
}

jboolean
isJFRRecordingStarted(J9JavaVM *vm)
{
	return vm->jfrState.isStarted ? JNI_TRUE : JNI_FALSE;
}

jboolean
isJFRCreated(J9JavaVM *vm)
{
	return vm->jfrState.isCreated ? JNI_TRUE : JNI_FALSE;
}

// TODO: add thread state parameter
void
jfrExecutionSample(J9VMThread *currentThread, J9VMThread *sampleThread)
{
#if defined(DEBUG)
	PORT_ACCESS_FROM_VMC(currentThread);
	j9tty_printf(PORTLIB, "\n!!! execution sample %p %p\n", currentThread, sampleThread);
#endif /* defined(DEBUG) */

	J9JFRExecutionSample *jfrEvent = (J9JFRExecutionSample *)reserveBufferWithStackTrace(currentThread, sampleThread, J9JFR_EVENT_TYPE_EXECUTION_SAMPLE, sizeof(*jfrEvent));
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

	double systemCPULoad = 0;
	intptr_t systemCPULoadRC = omrsysinfo_get_CPU_load(&systemCPULoad);

	if ((0 == processTimeRC) && (0 == systemCPULoadRC)) {
		J9JFRCPULoad *jfrEvent = (J9JFRCPULoad *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRCPULoad));
		if (NULL != jfrEvent) {
			initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_CPU_LOAD);

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

			jfrEvent->machineTotal = (float)systemCPULoad;
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
		J9JFRThreadCPULoad *jfrEvent = (J9JFRThreadCPULoad *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRThreadCPULoad));
		if (NULL != jfrEvent) {
			initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_THREAD_CPU_LOAD);

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
	J9JFRClassLoadingStatistics *jfrEvent = (J9JFRClassLoadingStatistics *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRClassLoadingStatistics));

	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_CLASS_LOADING_STATISTICS);

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
		J9JFRThreadContextSwitchRate *jfrEvent = (J9JFRThreadContextSwitchRate *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRThreadContextSwitchRate));
		if (NULL != jfrEvent) {
			JFRState *jfrState = &currentThread->javaVM->jfrState;
			int64_t currentTime = j9time_nano_time();

			initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_THREAD_CONTEXT_SWITCH_RATE);

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

typedef struct NetworkCallbackData {
	J9VMThread *currentThread;
	int64_t currentTime;
	int64_t prevTimestamp;
} NetworkCallbackData;

static JFRNetworkInterfaceStats *
findNetworkInterfaceStats(JFRNetworkInterfaceStats *head, const char *interfaceName)
{
	JFRNetworkInterfaceStats *current = head;
	while (NULL != current) {
		if (0 == strcmp(current->interfaceName, interfaceName)) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

static uintptr_t
networkStatsCallback(const char *interfaceName, uint64_t rxBytes, uint64_t txBytes, void *userData)
{
	NetworkCallbackData *callbackData = (NetworkCallbackData *)userData;
	J9VMThread *currentThread = callbackData->currentThread;
	J9JavaVM *vm = currentThread->javaVM;
	JFRState *jfrState = &vm->jfrState;
	PORT_ACCESS_FROM_VMC(currentThread);

	/* Look up previous stats for this interface in linked list. */
	JFRNetworkInterfaceStats *prevStats = findNetworkInterfaceStats((JFRNetworkInterfaceStats *)jfrState->networkInterfaceStatsList, interfaceName);

	J9JFRNetworkUtilization *jfrEvent = (J9JFRNetworkUtilization *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRNetworkUtilization));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_NETWORKUTILIZATION);

		/* Copy interface name to char array. */
		strncpy(jfrEvent->networkInterface, interfaceName, sizeof(jfrEvent->networkInterface) - 1);
		jfrEvent->networkInterface[sizeof(jfrEvent->networkInterface) - 1] = '\0';

		if ((NULL == prevStats) || (-1 == callbackData->prevTimestamp) || (callbackData->currentTime == callbackData->prevTimestamp)) {
			jfrEvent->readRate = 0;
			jfrEvent->writeRate = 0;
		} else {
			int64_t timeDelta = callbackData->currentTime - callbackData->prevTimestamp;
			/* Calculate bytes per second. */
			jfrEvent->readRate = I_64((rxBytes - prevStats->prevReadBytes) * 1e9 / timeDelta);
			jfrEvent->writeRate = I_64((txBytes - prevStats->prevWriteBytes) * 1e9 / timeDelta);
		}

		/* Update or create stats entry for this interface. */
		if (NULL == prevStats) {
			prevStats = (JFRNetworkInterfaceStats *)j9mem_allocate_memory(sizeof(JFRNetworkInterfaceStats), J9MEM_CATEGORY_JFR);
			if (NULL != prevStats) {
				strncpy(prevStats->interfaceName, interfaceName, sizeof(prevStats->interfaceName) - 1);
				prevStats->interfaceName[sizeof(prevStats->interfaceName) - 1] = '\0';
				prevStats->prevReadBytes = 0;
				prevStats->prevWriteBytes = 0;
				/* Add to head of linked list. */
				prevStats->next = (JFRNetworkInterfaceStats *)jfrState->networkInterfaceStatsList;
				jfrState->networkInterfaceStatsList = prevStats;
			}
		}
		if (NULL != prevStats) {
			prevStats->prevReadBytes = rxBytes;
			prevStats->prevWriteBytes = txBytes;
		}
	}

	return 0; /* Continue enumeration. */
}

static void
jfrNetworkUtilization(J9VMThread *currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	JFRState *jfrState = &currentThread->javaVM->jfrState;
	int64_t currentTime = j9time_nano_time();

	NetworkCallbackData callbackData;
	callbackData.currentThread = currentThread;
	callbackData.currentTime = currentTime;
	callbackData.prevTimestamp = jfrState->prevNetworkTimestamp;

	omrnetwork_get_network_interfaces(networkStatsCallback, &callbackData);

	jfrState->prevNetworkTimestamp = currentTime;
}

static void
jfrThreadStatistics(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9JFRThreadStatistics *jfrEvent = (J9JFRThreadStatistics *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRThreadStatistics));

	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_THREAD_STATISTICS);

		jfrEvent->activeThreadCount = vm->totalThreadCount;
		jfrEvent->daemonThreadCount = vm->daemonThreadCount;
		jfrEvent->accumulatedThreadCount = vm->accumulatedThreadCount;
		jfrEvent->peakThreadCount = vm->peakThreadCount;
	}

}

static int J9THREAD_PROC
jfrSamplingThreadProc(void *entryArg)
{
	J9JavaVM *vm = (J9JavaVM *)entryArg;
	J9VMThread *currentThread = NULL;

	if (JNI_OK == attachSystemDaemonThread(vm, &currentThread, "JFR sampler")) {
		omrthread_monitor_enter(vm->jfrSamplerMutex);
		vm->jfrSamplerState = J9JFR_SAMPLER_STATE_RUNNING;
		omrthread_monitor_notify_all(vm->jfrSamplerMutex);
		UDATA count = 0;
		while (J9JFR_SAMPLER_STATE_STOP != vm->jfrSamplerState) {
			J9SignalAsyncEvent(vm, NULL, vm->jfrAsyncKey);
			if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_JFR_V2_SUPPORT)) {
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
					if (0 == (count % 500)) { // 5 seconds
						jfrNetworkUtilization(currentThread);
					}
					internalReleaseVMAccess(currentThread);
					omrthread_monitor_enter(vm->jfrSamplerMutex);
				}
				count += 1;
			}
			omrthread_monitor_wait_timed(vm->jfrSamplerMutex, J9JFR_SAMPLING_RATE, 0);
		}
		omrthread_monitor_exit(vm->jfrSamplerMutex);
		DetachCurrentThread((JavaVM *)vm);
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
	if (isJFRV2SupportEnabled(vm)) {
		vm->jfrState.shouldRotateDisk = JNI_FALSE;
	}
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

void
enableJFRRecordingOnThread(J9VMThread *currentThread, j9object_t threadObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9OBJECT_I32_STORE(currentThread, threadObject, vm->isJFRExcludedOffset, FALSE);
}

void
disableJFRRecordingOnThread(J9VMThread *currentThread, j9object_t threadObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9OBJECT_I32_STORE(currentThread, threadObject, vm->isJFRExcludedOffset, TRUE);
}

BOOLEAN
isJFRRecordingDisabledOnThread(J9VMThread *currentThread, j9object_t threadObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	return J9OBJECT_I32_LOAD(currentThread, threadObject, vm->isJFRExcludedOffset);
}

static UDATA
jfrTypeIDHashFn(void *key, void *userData)
{
	const J9JFRTypeID *entry = (const J9JFRTypeID *)key;
	const J9UTF8 *className = entry->className;

	return (UDATA)VM_VMHelpers::computeHashForUTF8(J9UTF8_DATA(className), J9UTF8_LENGTH(className));
}

static UDATA
jfrTypeIDHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	const J9JFRTypeID *tableEntry = (const J9JFRTypeID *)tableNode;
	const J9JFRTypeID *queryEntry = (const J9JFRTypeID *)queryNode;

	return J9UTF8_EQUALS(tableEntry->className, queryEntry->className);
}

UDATA
initializeJFRIDs(J9JavaVM *vm)
{
	UDATA result = 0;

	if (omrthread_monitor_init_with_name(&vm->jfrState.typeIDMonitor, 0, "JFR Type ID monitor")) {
		result = 1;
		goto done;
	}

	vm->jfrState.typeIDcount = 0;

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

jlong
getTypeIdUTF8(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *className, BOOLEAN freeName)
{
	J9JavaVM *vm = currentThread->javaVM;
	jlong result = INVALID_TYPE_ID;

	Trc_VM_getTypeIdUTF8_Entry(currentThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className));
	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_JFR_V2_SUPPORT)) {
		result = getTypeIdImpl(currentThread, classLoader, className, freeName);
	}
	Trc_VM_getTypeIdUTF8_Exit(currentThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), NULL, result);

	return result;
}

jlong
getTypeId(J9VMThread *currentThread, J9Class *clazz)
{
	return getTypeIdImpl(currentThread, clazz->classLoader, J9ROMCLASS_CLASSNAME(clazz->romClass), FALSE);
}

static jlong
getTypeIdImpl(J9VMThread *currentThread, J9ClassLoader *classLoader, J9UTF8 *className, BOOLEAN freeName, J9Class *clazz)
{
	J9JavaVM *vm = currentThread->javaVM;
	jlong result = INVALID_TYPE_ID;
	J9JFRTypeID entry = {0};
	J9JFRTypeID *jfrTypeID = &entry;

	if (NULL != clazz) {
		if (NULL == vm->jfrState.jfrEventClassRef) {
			return -1;
		}
		J9Class *eventClass = J9VMJAVALANGCLASS_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(vm->jfrState.jfrEventClassRef));
		if (!isSameOrSuperClassOf(eventClass, clazz)
			|| J9_ARE_NO_BITS_SET(clazz->romClass->modifiers, J9AccAbstract)
		) {
			return -1;
		}
	}

	Assert_VM_mustHaveVMAccess(currentThread);

	omrthread_monitor_enter(vm->jfrState.typeIDMonitor);

	J9HashTable *typeIDTable = classLoader->typeIDs;

	if (NULL == typeIDTable) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		typeIDTable = hashTableNew(OMRPORT_FROM_J9PORT(privatePortLibrary),
								J9_GET_CALLSITE(),
								0,
								sizeof(J9JFRTypeID),
								sizeof(J9JFRTypeID *),
								0,
								J9MEM_CATEGORY_JFR,
								jfrTypeIDHashFn,
								jfrTypeIDHashEqualFn,
								NULL,
								NULL);

		if (NULL == typeIDTable) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
		classLoader->typeIDs = typeIDTable;

#if JAVA_SPEC_VERSION >= 17
		if (vm->systemClassLoader == classLoader) {
			/* Pre-populate table with known JFR event IDs */
			if (!addEventIds(vm)) {
				hashTableFree(typeIDTable);
				classLoader->typeIDs = NULL;
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto done;
			}

			/* Pre-populate table with known JFR type IDs */
			if (!addTypeIds(vm)) {
				hashTableFree(typeIDTable);
				classLoader->typeIDs = NULL;
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto done;
			}
		}
#endif /* JAVA_SPEC_VERSION >= 17 */

	classLoader->typeIDs = typeIDTable;
	}

	jfrTypeID->className = className;
	jfrTypeID = (J9JFRTypeID *)hashTableFind(typeIDTable, jfrTypeID);

	if (NULL == jfrTypeID) {
		/* Entry not found - add new entry with incremented counter */
		jfrTypeID = &entry;
		jfrTypeID->id = vm->jfrState.typeIDcount;
		jfrTypeID->free = freeName;
		jfrTypeID->isEvent = FALSE;
		jfrTypeID->eventClass = NULL;

		vm->jfrState.typeIDcount += 1;
		Assert_VM_true(vm->jfrState.typeIDcount > 0);

		jfrTypeID = (J9JFRTypeID *)hashTableAdd(typeIDTable, jfrTypeID);
		if (NULL == jfrTypeID) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	} else if (NULL != clazz) {
		jfrTypeID->isEvent = TRUE;
		jfrTypeID->eventClass = clazz;
	}
	result = jfrTypeID->id;

done:
	omrthread_monitor_exit(vm->jfrState.typeIDMonitor);

	return result;
}

jint
initializeJFRv2(J9JavaVM *vm)
{
	jint rc = JNI_ERR;
	J9HookInterface **vmHooks = getVMHookInterface(vm);

	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INITIALIZED, jfrCheckJFRCMDLineOptions, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}

	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, jfrShutdownInternalStructures, OMR_GET_CALLSITE(), NULL)) {
		goto done;
	}

	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_JFR_V2_SUPPORT)) {
		if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_INITIALIZE, jfrClassInitialize, OMR_GET_CALLSITE(), NULL)) {
			goto done;
		}
	}

	if (0 != initializeJFRIDs(vm)) {
		goto done;
	}

	rc = JNI_OK;

done:
	return rc;
}

void
jfrInitializeInternalStructures(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9Class *jfrInternalEventClass = NULL;
	J9Class *jfrEventClass = NULL;

	jfrInternalEventClass = vmFuncs->internalFindClassUTF8(currentThread, (U_8 *)J9UTF8_DATA(&jfrInternalEventClassUTF8), J9UTF8_LENGTH(&jfrInternalEventClassUTF8), vm->systemClassLoader, 0);
	Assert_VM_notNull(jfrInternalEventClass);

	jfrEventClass = vmFuncs->internalFindClassUTF8(currentThread, (U_8 *)J9UTF8_DATA(&jfrEventClassUTF8), J9UTF8_LENGTH(&jfrEventClassUTF8), vm->systemClassLoader, 0);
	Assert_VM_notNull(jfrEventClass);

	vm->jfrState.jfrInternalEventClassRef = (jclass) vmFuncs->j9jni_createGlobalRef((JNIEnv *)currentThread, jfrInternalEventClass->classObject, FALSE);
	vm->jfrState.jfrEventClassRef = (jclass) vmFuncs->j9jni_createGlobalRef((JNIEnv *)currentThread, jfrEventClass->classObject, FALSE);

	if ((NULL == vm->jfrState.jfrEventClassRef) || (NULL == vm->jfrState.jfrInternalEventClassRef)) {
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		vm->extendedRuntimeFlags3 |= J9_EXTENDED_RUNTIME3_ENABLE_JFR_CLASSLOAD_TRANSFORM;
	}

	return;
}

static void
jfrShutdownInternalStructures(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThread *currentThread = ((J9VMInitEvent *)eventData)->vmThread;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	vm->extendedRuntimeFlags3 &= ~J9_EXTENDED_RUNTIME3_ENABLE_JFR_CLASSLOAD_TRANSFORM;

	internalAcquireVMAccess(currentThread);
	vmFuncs->j9jni_deleteGlobalRef((JNIEnv *)currentThread, vm->jfrState.jfrEventClassRef, FALSE);
	vmFuncs->j9jni_deleteGlobalRef((JNIEnv *)currentThread, vm->jfrState.jfrInternalEventClassRef, FALSE);
	vmFuncs->j9jni_deleteGlobalRef((JNIEnv *)currentThread, vm->jfrState.chunkRotationMonitor, FALSE);
	internalReleaseVMAccess(currentThread);
	vm->jfrState.jfrEventClassRef = NULL;
	vm->jfrState.jfrInternalEventClassRef = NULL;
	vm->jfrState.chunkRotationMonitor = NULL;
	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_JFR_V2_SUPPORT)) {
		(*hook)->J9HookUnregister(hook, J9HOOK_VM_CLASS_INITIALIZE, jfrClassInitialize, NULL);
	}
}

static void
jfrCheckJFRCMDLineOptions(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMThread *currentThread = ((J9VMInitEvent *)eventData)->vmThread;

	internalAcquireVMAccess(currentThread);
	runStaticMethod(currentThread, (U_8 *)"java/lang/JFRHelpers", (J9NameAndSignature *)&initJFRNAS, 0, NULL);
	internalReleaseVMAccess(currentThread);
}

void
jvmUpcallsEagerByteInstrumentation(J9VMThread *currentThread, J9Class *superClass, U_8 *className, U_16 classNameLength, J9ClassLoader *loader, U_8 *classData, UDATA classDataLength, U_8 **newClassData, UDATA *newClassDataLength)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9MemoryManagerFunctions *mmfns = vm->memoryManagerFunctions;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jlong traceID = 0;
	j9object_t inputByteArray = NULL;
	j9object_t outputByteArray = NULL;
	UDATA args[6];
	BOOLEAN freeName = FALSE;

	U_8 buf[J9JFR_CLASSNAME_BUFFER_SIZE + sizeof(U_16)];
	J9UTF8 *nameUTF8 = (J9UTF8 *)buf;

	if (classNameLength > J9JFR_CLASSNAME_BUFFER_SIZE) {
		nameUTF8 = (J9UTF8 *)j9mem_allocate_memory(classNameLength + sizeof(U_16), OMRMEM_CATEGORY_VM);
		if (NULL == nameUTF8) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
		freeName = TRUE;
	}
	J9UTF8_LENGTH(nameUTF8) = (U_16)classNameLength;
	memcpy(J9UTF8_DATA(nameUTF8), className, classNameLength);

	traceID = getTypeIdUTF8(currentThread, loader, nameUTF8, freeName);

	Assert_VM_true(-1 != traceID);

	inputByteArray = mmfns->J9AllocateIndexableObject(currentThread, vm->byteReflectClass->arrayClass, (U_32)classDataLength, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == inputByteArray) {
		vmFuncs->setHeapOutOfMemoryError(currentThread);
		goto done;
	}

	VM_ArrayCopyHelpers::memcpyToArray(currentThread, inputByteArray, (UDATA)0, classDataLength, (void *)classData);

	if (NULL == vm->jfrState.onRetransformUpcallMethod) {
		PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, inputByteArray);
		J9Class *jfrHelpersClass = vmFuncs->internalFindClassUTF8(currentThread, (U_8 *)J9UTF8_DATA(&jfrHelpersUTF8), J9UTF8_LENGTH(&jfrHelpersUTF8), vm->systemClassLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
		if (NULL == jfrHelpersClass) {
			goto popInputArrayAndDone;
		}
		vm->jfrState.onRetransformUpcallMethod = (J9Method *)vmFuncs->javaLookupMethodImpl(currentThread, jfrHelpersClass, (J9ROMNameAndSignature *)&bytesForEagerInstrumentationNAS, jfrHelpersClass, J9_LOOK_STATIC | J9_LOOK_DIRECT_NAS, NULL);
		if (NULL == vm->jfrState.onRetransformUpcallMethod) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			goto popInputArrayAndDone;
		}
		inputByteArray = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	}
	args[0] = 0;
	args[1] = (UDATA)traceID;
	args[2] = (UDATA)FALSE;
	args[3] = (UDATA)superClass->classObject;
	args[4] = (UDATA)inputByteArray;
	args[5] = (UDATA)(BOOLEAN)!isSameOrSuperClassOf(J9VMJAVALANGCLASS_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(vm->jfrState.jfrEventClassRef)), superClass);

	vmFuncs->internalRunStaticMethod(currentThread, vm->jfrState.onRetransformUpcallMethod, TRUE, 6, args);
	outputByteArray = (j9object_t)currentThread->returnValue;

	if (VM_VMHelpers::exceptionPending(currentThread) || (NULL == outputByteArray)) {
		goto done;
	}

	*newClassDataLength = (jint)J9INDEXABLEOBJECT_SIZE(currentThread, outputByteArray);
	*newClassData = (U_8 *)j9mem_allocate_memory(*newClassDataLength, OMRMEM_CATEGORY_VM);
	if (NULL == *newClassData) {
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		goto done;
	}

	VM_ArrayCopyHelpers::memcpyFromArray(currentThread, outputByteArray, (UDATA)0, (UDATA)0, *newClassDataLength, *newClassData);

done:
	return;

popInputArrayAndDone:
	inputByteArray = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	goto done;
}

j9object_t
jvmUpcallTransformArrayToList(J9VMThread *currentThread, j9object_t array)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	j9object_t result = NULL;
	UDATA args[1];

	if (NULL == vm->jfrState.transformToListMethod) {
		J9Class *jfrHelpersClass = vmFuncs->internalFindClassUTF8(currentThread, (U_8 *)J9UTF8_DATA(&jfrHelpersUTF8), J9UTF8_LENGTH(&jfrHelpersUTF8), vm->systemClassLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);

		vm->jfrState.transformToListMethod = (J9Method *)vmFuncs->javaLookupMethodImpl(currentThread, jfrHelpersClass, (J9ROMNameAndSignature *)&transformToListNAS, jfrHelpersClass, J9_LOOK_STATIC | J9_LOOK_DIRECT_NAS, NULL);
		if (NULL == vm->jfrState.transformToListMethod) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			goto done;
		}
	}

	args[0] = (UDATA)array;
	vmFuncs->internalRunStaticMethod(currentThread, vm->jfrState.transformToListMethod, TRUE, 1, args);
	result = (j9object_t)currentThread->returnValue;

done:
	return result;
}

#if JAVA_SPEC_VERSION >= 17
/**
 * Helper function to add a JFR event or type to the hash table.
 * This function is called by addEventIds() and addTypeIds() functions.
 *
 * @param vm[in] the J9JavaVM
 * @param eventOrTypeName[in] the name of the event or type
 * @param id[in] the ID to assign
 * @param isEvent[in] true if this is an event, false if it's a type
 *
 * @returns true on success, false on failure
 */
static bool
addType(J9JavaVM *vm, const char *eventOrTypeName, UDATA id, bool isEvent)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9ClassLoader *classLoader = vm->systemClassLoader;
	J9HashTable *typeIDTable = classLoader->typeIDs;
	bool result = false;
	J9UTF8 *className = NULL;

	/* typeIDTable should already be created by caller */
	Assert_VM_notNull(typeIDTable);

	J9JFRTypeID entry = {0};
	UDATA nameLength = strlen(eventOrTypeName);

	/* Create J9UTF8 structure for the event/type name */
	className = (J9UTF8 *)j9mem_allocate_memory(
		sizeof(J9UTF8) + nameLength, J9MEM_CATEGORY_JFR);
	if (NULL == className) {
		goto done;
	}

	J9UTF8_SET_LENGTH(className, (U_16)nameLength);
	memcpy(J9UTF8_DATA(className), eventOrTypeName, nameLength);

	vm->jfrState.typeIDcount = OMR_MAX(vm->jfrState.typeIDcount, (jlong)id);

	/* Populate J9JFRTypeID entry */
	entry.id = id;
	entry.className = className;
	entry.free = TRUE;
	entry.isEvent = isEvent;
	entry.eventClass = NULL;

	/* Add to hash table */
	if (NULL == hashTableAdd(typeIDTable, &entry)) {
		j9mem_free_memory(className);
		goto done;
	}

	result = true;

done:
	return result;
}

static bool
addEventIds(J9JavaVM *vm) {
	bool result = true;
	const size_t numEvents = sizeof(EVENT_MAPPINGS) / sizeof(EVENT_MAPPINGS[0]);
	for (size_t i = 0; i < numEvents; i++) {
		if (!addType(vm, EVENT_MAPPINGS[i].name, EVENT_MAPPINGS[i].id, true)) {
			result = false;
			break;
		}
	}
	return result;
}

static bool
addTypeIds(J9JavaVM *vm) {
	bool result = true;
	const size_t numTypes = sizeof(TYPE_MAPPINGS) / sizeof(TYPE_MAPPINGS[0]);
	for (size_t i = 0; i < numTypes; i++) {
		if (!addType(vm, TYPE_MAPPINGS[i].name, TYPE_MAPPINGS[i].id, false)) {
			result = false;
			break;
		}
	}
	return result;
}

jboolean
JfrPeriodicEventSet::requestEvent(J9VMThread *currentThread, jlong id)
{
	switch (id) {
	case JfrClassLoaderStatisticsEvent:
		requestClassLoaderStatistics(currentThread);
		break;
	case JfrClassLoadingStatisticsEvent:
		requestClassLoadingStatistics(currentThread);
		break;
	case JfrCPUInformationEvent:
		requestCPUInformation(currentThread);
		break;
	case JfrCPULoadEvent:
		requestCPULoad(currentThread);
		break;
	case JfrExecutionSampleEvent:
		requestExecutionSample(currentThread);
		break;
	case JfrGCHeapConfigurationEvent:
		requestGCHeapConfiguration(currentThread);
		break;
	case JfrInitialEnvironmentVariableEvent:
		requestInitialEnvironmentVariable(currentThread);
		break;
	case JfrInitialSystemPropertyEvent:
		requestInitialSystemProperty(currentThread);
		break;
	case JfrJavaThreadStatisticsEvent:
		requestJavaThreadStatistics(currentThread);
		break;
	case JfrJVMInformationEvent:
		requestJVMInformation(currentThread);
		break;
	case JfrModuleExportEvent:
		requestModuleExport(currentThread);
		break;
	case JfrModuleRequireEvent:
		requestModuleRequire(currentThread);
		break;
	case JfrNativeLibraryEvent:
		requestNativeLibrary(currentThread);
		break;
	case JfrNetworkUtilizationEvent:
		requestNetworkUtilization(currentThread);
		break;
	case JfrOSInformationEvent:
		requestOSInformation(currentThread);
		break;
	case JfrPhysicalMemoryEvent:
		requestPhysicalMemory(currentThread);
		break;
	case JfrSystemProcessEvent:
		requestSystemProcess(currentThread);
		break;
	case JfrThreadContextSwitchRateEvent:
		requestThreadContextSwitchRate(currentThread);
		break;
	case JfrThreadCPULoadEvent:
		requestThreadCPULoad(currentThread);
		break;
	case JfrThreadDumpEvent:
		requestThreadDump(currentThread);
		break;
	case JfrVirtualizationInformationEvent:
		requestVirtualizationInformation(currentThread);
		break;
	case JfrYoungGenerationConfigurationEvent:
		requestYoungGenerationConfiguration(currentThread);
		break;
	default:
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

void
JfrPeriodicEventSet::requestJVMInformation(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_JVM_INFORMATION);
	}
}

void
JfrPeriodicEventSet::requestOSInformation(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_OS_INFORMATION);
	}
}

void
JfrPeriodicEventSet::requestVirtualizationInformation(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_VIRTUALIZATION_INFORMATION);
	}
}

void
JfrPeriodicEventSet::requestInitialSystemProperty(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_INITIAL_SYSTEM_PROPERTY);
	}
}

void
JfrPeriodicEventSet::requestInitialEnvironmentVariable(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_INITIAL_ENVIRONMENT_VARIABLE);
	}
}

void
JfrPeriodicEventSet::requestSystemProcess(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_SYSTEM_PROCESS);
	}
}

void
JfrPeriodicEventSet::requestCPUInformation(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_CPU_INFORMATION);
	}
}

void
JfrPeriodicEventSet::requestCPULoad(J9VMThread *currentThread)
{
	jfrCPULoad(currentThread);
}

void
JfrPeriodicEventSet::requestThreadCPULoad(J9VMThread *currentThread)
{
	jfrThreadCPULoad(currentThread, currentThread);
}

void
JfrPeriodicEventSet::requestThreadContextSwitchRate(J9VMThread *currentThread)
{
	jfrThreadContextSwitchRate(currentThread);
}

void
JfrPeriodicEventSet::requestNetworkUtilization(J9VMThread *currentThread)
{
	jfrNetworkUtilization(currentThread);
}

void
JfrPeriodicEventSet::requestJavaThreadStatistics(J9VMThread *currentThread)
{
	jfrThreadStatistics(currentThread);
}

void
JfrPeriodicEventSet::requestClassLoadingStatistics(J9VMThread *currentThread)
{
	jfrClassLoadingStatistics(currentThread);
}

void
JfrPeriodicEventSet::requestClassLoaderStatistics(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_CLASS_LOADER_STATISTICS);
	}
}

void
JfrPeriodicEventSet::requestPhysicalMemory(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_PHYSICAL_MEMORY);
	}
}

void
JfrPeriodicEventSet::requestExecutionSample(J9VMThread *currentThread)
{
	jfrExecutionSample(currentThread, currentThread);
}

void
JfrPeriodicEventSet::requestThreadDump(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_THREAD_DUMP);
	}
}

void
JfrPeriodicEventSet::requestNativeLibrary(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_NATIVE_LIBRARY);
	}
}

void
JfrPeriodicEventSet::requestModuleRequire(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_MODULE_REQUIRE);
	}
}

void
JfrPeriodicEventSet::requestModuleExport(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_MODULE_EXPORT);
	}
}

void
JfrPeriodicEventSet::requestGCHeapConfiguration(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_GC_HEAP_CONFIGURATION);
	}
}

void
JfrPeriodicEventSet::requestYoungGenerationConfiguration(J9VMThread *currentThread)
{
	J9JFREvent *jfrEvent = (J9JFREvent *)reserveBuffer(currentThread, currentThread, sizeof(J9JFREvent));
	if (NULL != jfrEvent) {
		initializeEventFields(currentThread, currentThread, jfrEvent, J9JFR_EVENT_TYPE_YOUNG_GENERATION_CONFIGURATION);
	}
}
#endif /* JAVA_SPEC_VERSION >= 17 */

void
jfrEmitDataLoss(J9VMThread *currentThread, U_64 bytes)
{
	J9JFRDataLoss *jfrEvent = (J9JFRDataLoss *)reserveBuffer(currentThread, currentThread, sizeof(J9JFRDataLoss));
	if (NULL != jfrEvent) {
		currentThread->threadJfrState.dataLostTotal += bytes;
		initializeEventFields(currentThread, currentThread, (J9JFREvent *)jfrEvent, J9JFR_EVENT_TYPE_DATA_LOSS);
		jfrEvent->amount = bytes;
		jfrEvent->total = currentThread->threadJfrState.dataLostTotal;
	}
}

jboolean
requestJFREvent(J9VMThread *currentThread, jlong id)
{
#if JAVA_SPEC_VERSION >= 17
	return JfrPeriodicEventSet::requestEvent(currentThread, id);
#else /* JAVA_SPEC_VERSION >= 17 */
	return JNI_FALSE;
#endif /* JAVA_SPEC_VERSION >= 17 */
}

BOOLEAN
setupChunkMonitor(J9VMThread *currentThread)
{
	BOOLEAN rc = TRUE;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	j9object_t chunkMonitor = VM_VMHelpers::getStaticFieldObject(currentThread, "jdk/jfr/internal/JVM", "CHUNK_ROTATION_MONITOR", "Ljava/lang/Object;");
	vm->jfrState.chunkRotationMonitor = vmFuncs->j9jni_createGlobalRef((JNIEnv *)currentThread, chunkMonitor, FALSE);

	if (NULL == vm->jfrState.chunkRotationMonitor) {
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		rc = FALSE;
	}

	vm->jfrState.shouldRotateDisk = JNI_FALSE;

	return rc;
}

static void
notifyForChunkRotation(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	omrthread_monitor_t monitorPtr = NULL;

	if (JNI_FALSE == vm->jfrState.shouldRotateDisk) {
#if defined(DEBUG)
		PORT_ACCESS_FROM_VMC(currentThread);
		j9tty_printf(PORTLIB, "\n!!! notifyForChunkRotation called\n");
#endif /* defined(DEBUG) */

		j9object_t chunkMonitor = J9_JNI_UNWRAP_REFERENCE(vm->jfrState.chunkRotationMonitor);
		VM_ObjectMonitor::enterObjectMonitor(currentThread, chunkMonitor);
		VM_ObjectMonitor::getMonitorForNotify(currentThread, chunkMonitor, &monitorPtr, false);

		if (NULL == monitorPtr) {
			/* If the monitor doesnt exist then no one is waiting on it. */
		} else {
			vm->jfrState.shouldRotateDisk = JNI_TRUE;
			issueWriteBarrier();
			omrthread_monitor_notify_all(monitorPtr);
		}
		VM_ObjectMonitor::exitObjectMonitor(currentThread, chunkMonitor);
	}
}

static void
checkAvailableSpaceInGlobalBuffer(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;

	if (isJFRV2SupportEnabled(vm)) {
		if (vm->jfrBuffer.bufferRemaining < (J9JFR_GLOBAL_BUFFER_SIZE/8)) {
			notifyForChunkRotation(currentThread);
		}
	}
}

void
jfrClassInitialize(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	J9VMClassInitializeEvent *event = (J9VMClassInitializeEvent *)eventData;
	J9VMThread *currentThread = event->currentThread;
	J9Class *clazz = event->clazz;

	/* getTypeIdImpl will add clazz to the TypeID table. */
	getTypeIdImpl(currentThread, clazz->classLoader, J9ROMCLASS_CLASSNAME(clazz->romClass), FALSE, clazz);
}

} /* extern "C" */

#endif /* defined(J9VM_OPT_JFR) */
