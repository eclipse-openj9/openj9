/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/* #define J9VM_DBG */

#include <string.h>

#include "omrcfg.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9cp.h"
#include "omrgcconsts.h"
#include "omrlinkedlist.h"
#include "j9port.h"
#include "j9protos.h"
#include "omrthread.h"
#include "j9vmnls.h"
#include "jni.h"
#include "monhelp.h"
#include "objhelp.h"
#include "omr.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "vmaccess.h"
#include "vmhook_internal.h"

#include "HeapIteratorAPI.h"
#include "j2sever.h"

#if defined(J9VM_OPT_CRIU_SUPPORT)
#include "CRIUHelpers.hpp"
#include "VMHelpers.hpp"
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

extern "C" {

#define SIGQUIT_FILE_NAME "sigquit"
#define SIGQUIT_FILE_EXT ".trc"

/* Generic rounding macro - result is a UDATA */
#define ROUND_TO(granularity, number) (((UDATA)(number) + (granularity) - 1) & ~((UDATA)(granularity) - 1))

#define LEADING_SPACE "   "
#define LEADING_SPACE_EXTRA "      "

static char *getJ9ThreadStatus(J9VMThread *vmThread);
static void printJ9ThreadStatusMonitorInfo (J9VMThread *vmStruct, IDATA tracefd);
static void trace_printf (struct J9PortLibrary *portLib, IDATA tracefd, char * format, ...);
static UDATA printMethodInfo (J9VMThread *currentThread , J9StackWalkState *stackWalkState);

#if defined(J9ZOS390)
static IDATA setFailedToForkThreadException(J9VMThread *currentThread, IDATA retVal, omrthread_os_errno_t os_errno, omrthread_os_errno_t os_errno2);
#else /* !J9ZOS390 */
static IDATA setFailedToForkThreadException(J9VMThread *currentThread, IDATA retVal, omrthread_os_errno_t os_errno);
#endif /* !J9ZOS390 */

static UDATA javaProtectedThreadProc (J9PortLibrary* portLibrary, void * entryarg);

static UDATA    startJavaThreadInternal(J9VMThread * currentThread, UDATA privateFlags, UDATA osStackSize, UDATA priority, omrthread_entrypoint_t entryPoint, void * entryArg, UDATA setException);

#if (defined(J9VM_DBG))
static void badness (char *description);
#endif /* J9VM_DBG */

static void dumpThreadingInfo(J9JavaVM *vm);
static void initMinCPUSpinCounts(J9JavaVM *vm);
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
static void printCustomSpinOptions(void *element, void *userData);
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

J9VMThread *
allocateVMThread(J9JavaVM *vm, omrthread_t osThread, UDATA privateFlags, void *memorySpace, J9Object *threadObject)
{
	PORT_ACCESS_FROM_PORT(vm->portLibrary);
	J9JavaStack *stack = NULL;
	J9VMThread *newThread = NULL;
	BOOLEAN threadIsRecycled = FALSE;
	J9MemoryManagerFunctions* gcFuncs = vm->memoryManagerFunctions;

#ifdef J9VM_INTERP_GROWABLE_STACKS
#define VMTHR_INITIAL_STACK_SIZE ((vm->initialStackSize > (UDATA) vm->stackSize) ? vm->stackSize : vm->initialStackSize)
#else
#define VMTHR_INITIAL_STACK_SIZE vm->stackSize
#endif

	omrthread_monitor_enter(vm->vmThreadListMutex);

	/* Allocate the stack */

	if ((stack = allocateJavaStack(vm, VMTHR_INITIAL_STACK_SIZE, NULL)) == NULL) {
		goto fail;
	}

#undef VMTHR_INITIAL_STACK_SIZE

	/* Try to reuse a dead thread; otherwise allocate a new one */
	if (J9_LINKED_LIST_IS_EMPTY(vm->deadThreadList)) {

		/* Create the vmThread */
		void *startOfMemoryBlock = NULL;
		UDATA vmThreadAllocationSize = J9VMTHREAD_ALIGNMENT + ROUND_TO(sizeof(UDATA), vm->vmThreadSize);
		if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
			startOfMemoryBlock = (void *)j9mem_allocate_memory32(vmThreadAllocationSize, OMRMEM_CATEGORY_THREADS);
		} else {
			startOfMemoryBlock = (void *)j9mem_allocate_memory(vmThreadAllocationSize, OMRMEM_CATEGORY_THREADS);
		}
		if (NULL == startOfMemoryBlock) {
			goto fail;
		}

		/* Align thread address to J9VMTHREAD_ALIGNMENT (~ 256 bytes) to prevent VM ACCESS errors */
		newThread = (J9VMThread *)ROUND_TO(J9VMTHREAD_ALIGNMENT, (UDATA)startOfMemoryBlock);

		/* Clean allocated memory and store alignment offset to retrieve original pointer for freeing memory */
		memset(newThread, 0, vm->vmThreadSize);
		newThread->startOfMemoryBlock = startOfMemoryBlock;

#if defined(J9VM_PORT_RUNTIME_INSTRUMENTATION)
		/* Allocate J9RIParameters. */
		newThread->riParameters = (J9RIParameters*)j9mem_allocate_memory(sizeof(J9RIParameters), OMRMEM_CATEGORY_THREADS);
		if (NULL == newThread->riParameters) {
			goto fail;
		}
		memset(newThread->riParameters, 0, sizeof(J9RIParameters));
#endif /* defined(J9VM_PORT_RUNTIME_INSTRUMENTATION) */

		/* Initialize the vmThread */

		/* Link the thread in the linked list - early, but done under mutex so we're safe*/
		J9_LINKED_LIST_ADD_LAST(vm->mainThread, newThread);

		omrthread_monitor_init_with_name(&newThread->publicFlagsMutex, J9THREAD_MONITOR_JLM_TIME_STAMP_INVALIDATOR, "Thread public flags mutex");
		if (newThread->publicFlagsMutex == NULL) {
			goto fail;
		}
		initOMRVMThread(vm, newThread);
	} else {

		/* Reuse a dead vmThread */
		threadIsRecycled = TRUE;

		/* Grab the first dead thread (already reinitialized) */
		J9_LINKED_LIST_REMOVE_FIRST(vm->deadThreadList, newThread);

		/* Link the thread in the linked list - early, but done under mutex so we're safe*/
		J9_LINKED_LIST_ADD_LAST(vm->mainThread, newThread);

		/* dead threads are stored in "halted for inspection" state. Resume the thread before we recycle it */
		omrthread_monitor_enter(newThread->publicFlagsMutex);
		if (newThread->inspectionSuspendCount != 0) {
			if (--newThread->inspectionSuspendCount == 0) {
				clearHaltFlag(newThread, J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION);
			}
		}
		omrthread_monitor_exit(newThread->publicFlagsMutex);
	}

	if (0 != vm->segregatedAllocationCacheSize) {
		newThread->segregatedAllocationCache = (J9VMGCSegregatedAllocationCacheEntry *)(((UDATA)newThread) + J9_VMTHREAD_SEGREGATED_ALLOCATION_CACHE_OFFSET);
	}

#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	newThread->compressObjectReferences = J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
	newThread->invokedCalldisp = FALSE;
#endif
	newThread->threadObject = threadObject;
	newThread->stackWalkState = &(newThread->inlineStackWalkState);
	newThread->javaVM = vm;

	newThread->contiguousIndexableHeaderSize = vm->contiguousIndexableHeaderSize;
	newThread->discontiguousIndexableHeaderSize = vm->discontiguousIndexableHeaderSize;
	newThread->unsafeIndexableHeaderSize = vm->unsafeIndexableHeaderSize;
#if defined(J9VM_ENV_DATA64)
	newThread->isIndexableDataAddrPresent = vm->isIndexableDataAddrPresent;
	newThread->isVirtualLargeObjectHeapEnabled = vm->isVirtualLargeObjectHeapEnabled;
#endif /* defined(J9VM_ENV_DATA64) */

	newThread->privateFlags = privateFlags;
	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		setEventFlag(newThread, J9_PUBLIC_FLAGS_DEBUG_VM_ACCESS);
	}
	newThread->stackObject = stack;
	newThread->stackOverflowMark = newThread->stackOverflowMark2 = J9JAVASTACK_STACKOVERFLOWMARK(stack);
	newThread->osThread = osThread;
#if defined(J9VM_OPT_CRIU_SUPPORT)
	/* JDWP threads need to remain live while checkpoint/restore hooks run, so add
	 * J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT to the new J9VMThread created from a
	 * JDWP java thread object.
	 */
	if (J9_ARE_ANY_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
		for (UDATA i = 0; i < vm->checkpointState.javaDebugThreadCount; i++) {
			j9object_t jdwpThreadObject = J9_JNI_UNWRAP_REFERENCE(vm->checkpointState.javaDebugThreads[i]);
			if (jdwpThreadObject == threadObject) {
				Trc_VM_criu_allocateVMThread_set_delayflag(i);
				newThread->privateFlags2 |= J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT;
				break;
			}
		}
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
	newThread->javaOffloadState = 0;
#endif

#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
	newThread->systemStackPointer = 0;
#endif

	/* Initialize stack and bytecode execution stuff (this flushes the method cache) */
	initializeExecutionModel(newThread);

#ifdef J9VM_OPT_SIDECAR
	/* Initialize fields used by java.lang.management */
	newThread->mgmtBlockedCount = 0;
	newThread->mgmtWaitedCount  = 0;
	newThread->mgmtBlockedStart = JNI_FALSE;
	newThread->mgmtWaitedStart = JNI_FALSE;
#endif

#ifdef OMR_GC_CONCURRENT_SCAVENGER
	/* Initialize fields used by Concurrent Scavenger */
	newThread->readBarrierRangeCheckBase = UDATA_MAX;
	newThread->readBarrierRangeCheckTop = 0;
#ifdef OMR_GC_COMPRESSED_POINTERS
	/* No need for a runtime check here - it would just waste cycles */
	newThread->readBarrierRangeCheckBaseCompressed = U_32_MAX;
	newThread->readBarrierRangeCheckTopCompressed = 0;
#endif /* OMR_GC_COMPRESSED_POINTERS */
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	/* Attach the thread to OMR */
	if (JNI_OK != attachVMThreadToOMR(vm, newThread, osThread)) {
		goto fail;
	}

	newThread->monitorEnterRecordPool = pool_new(sizeof(J9MonitorEnterRecord), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(PORTLIB));
	if (NULL == newThread->monitorEnterRecordPool) {
		goto fail;
	}

	newThread->omrVMThread->memorySpace = memorySpace;

	/* Initialize the thread for memory management purposes (vm->memoryManagerFunctions will be NULL if we failed to load the gc dll) */
	if ( (NULL == gcFuncs) || (0 != gcFuncs->initializeMutatorModelJava(newThread)) ) {
		goto fail;
	}

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	newThread->jitCountDelta = 2;
	newThread->maxProfilingCount = (3000 * 2) + 1;
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE)
	/* Propagate TOC/GOT register into threads as they are created */
	newThread->jitTOC = vm->jitTOC;
#endif
#endif

#if JAVA_SPEC_VERSION >= 16
	newThread->ffiArgs = NULL;
	newThread->ffiArgCount = 0;
	newThread->jmpBufEnvPtr = NULL;
#endif /* JAVA_SPEC_VERSION >= 16 */
#if JAVA_SPEC_VERSION >= 21
	newThread->isInCriticalDownCall = FALSE;
#endif /* JAVA_SPEC_VERSION >= 21 */

#if JAVA_SPEC_VERSION >= 19
	newThread->currentContinuation = NULL;
	newThread->continuationPinCount = 0;
	newThread->ownedMonitorCount = 0;
	newThread->callOutCount = 0;
	newThread->carrierThreadObject = threadObject;
	newThread->scopedValueCache = NULL;
#endif /* JAVA_SPEC_VERSION >= 19 */

	/* If an exclusive access request is in progress, mark this thread */

	omrthread_monitor_enter(vm->exclusiveAccessMutex);
	/* The new thread does not have VM access, so there's no need to set any not_counted
	 * bits, as the thread will block attempting to acquire VM access, and will not release
	 * VM access for the duration of the exclusive.
	 */
	if (J9_XACCESS_NONE != vm->exclusiveAccessState) {
		setHaltFlag(newThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE);
	}
	if (J9_XACCESS_NONE != vm->safePointState) {
		setHaltFlag(newThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT);
	}
#if defined(J9VM_OPT_CRIU_SUPPORT)
	if (VM_CRIUHelpers::isJVMInSingleThreadMode(vm) && VM_VMHelpers::threadCanRunJavaCode(newThread)) {
		/* New threads with the delay halt flag should not be halted here. */
		Trc_VM_criu_allocateVMThread_check_delayflag(newThread);
		if (J9_ARE_NO_BITS_SET(newThread->privateFlags2, J9_PRIVATE_FLAGS2_DELAY_HALT_FOR_CHECKPOINT)) {
			Trc_VM_criu_allocateVMThread_set_haltflag(newThread);
			setHaltFlag(newThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
		}
	}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	omrthread_monitor_exit(vm->exclusiveAccessMutex);

	/* Set the thread's method enter notification bits */

	newThread->eventFlags = vm->globalEventFlags;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_THREAD_CREATED)) {
		UDATA continueInitialization = TRUE;

		ALWAYS_TRIGGER_J9HOOK_VM_THREAD_CREATED(vm->hookInterface, newThread, continueInitialization);

		if (!continueInitialization) {
			/* Make sure the memory manager does anything needed before shutting down */
			/* Holding the vmThreadListMutex ensures that no heap walking will occur, ergo heap manipulation is safe */
			gcFuncs->cleanupMutatorModelJava(newThread);
			TRIGGER_J9HOOK_VM_THREAD_DESTROY(vm->hookInterface, newThread);
			goto fail;
		}
	}

	/* Update counters for total # of threads and daemon threads and notify anyone waiting */

	++(vm->totalThreadCount);
	if (privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) {
		++(vm->daemonThreadCount);
	}
	omrthread_monitor_notify_all(vm->vmThreadListMutex);

	omrthread_monitor_exit(vm->vmThreadListMutex);
	return newThread;

fail:
	if (stack) {
		freeJavaStack(vm, stack);
	}
	if (newThread) {
		J9Pool *pool = newThread->monitorEnterRecordPool;
		if (NULL != pool) {
			newThread->monitorEnterRecordPool = NULL;
			pool_kill(pool);
		}

		/* Remove the TLS entry for this thread to prevent currentVMThread(vm) from finding freed memory */
		omrthread_tls_set(osThread, vm->omrVM->_vmThreadKey, NULL);

		/* Remove the thread from the live thread list */
		J9_LINKED_LIST_REMOVE(vm->mainThread, newThread);

		/* Return the new thread to the deadThreadList if it came from there */
		if (threadIsRecycled) {
			J9_LINKED_LIST_ADD_LAST(vm->deadThreadList, newThread);
		} else {
			if (newThread->publicFlagsMutex) {
				omrthread_monitor_destroy(newThread->publicFlagsMutex);
			}
			freeVMThread(vm, newThread);
		}
		newThread->threadObject = NULL;
#if JAVA_SPEC_VERSION >= 19
		newThread->carrierThreadObject = NULL;
#endif /* JAVA_SPEC_VERSION >= 19 */
		/* Detach the thread from OMR */
		detachVMThreadFromOMR(vm, newThread);
		if (!threadIsRecycled) {
			destroyOMRVMThread(vm, newThread);
		}
	}
	omrthread_monitor_exit(vm->vmThreadListMutex);
	return NULL;
}



IDATA J9THREAD_PROC javaThreadProc(void *entryarg)
{
	J9JavaVM * vm = (J9JavaVM*)entryarg;
	J9VMThread* vmThread = currentVMThread(vm);
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA result;

	vmThread->gpProtected = 1;

	j9sig_protect(javaProtectedThreadProc, vmThread,
		structuredSignalHandler, vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&result);

	exitJavaThread(vm);
	/* Execution never reaches this point */
	return 0;
}



#if (defined(J9VM_DBG))
static void badness(char *description)
{
	printf("\n<badness: %s>\n", description);
}
#endif /* J9VM_DBG */


void OMRNORETURN
exitJavaThread(J9JavaVM * vm)
{
	omrthread_monitor_enter(vm->vmThreadListMutex);
	--(vm->zombieThreadCount);
	omrthread_monitor_notify_all(vm->vmThreadListMutex);
	omrthread_exit(vm->vmThreadListMutex);
	/* Execution never reaches this point */
}

J9VMThread *
currentVMThread(J9JavaVM *vm)
{
	return getVMThreadFromOMRThread(vm, omrthread_self());
}


void threadCleanup(J9VMThread * vmThread, UDATA forkedByVM)
{
	J9JavaVM * vm = vmThread->javaVM;

	enterVMFromJNI(vmThread);
	/* Inform ThreadGroup about any uncaught exception.  Tiny VMs do not have ThreadGroup, so they just dump the exception. */
	if (vmThread->currentException) {
		handleUncaughtException(vmThread);
		/* Safe to call this whether handleUncaughtException clears the exception or not */
		internalExceptionDescribe(vmThread);
	}
	releaseVMAccess(vmThread);

	/* Mark this thread as dead */
	setEventFlag(vmThread, J9_PUBLIC_FLAGS_STOPPED);

	/* We are dead at this point. Clear the suspend bit prior to triggering the thread end hook */
	clearHaltFlag(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);

	TRIGGER_J9HOOK_VM_THREAD_END(vmThread->javaVM->hookInterface, vmThread, 0);

#ifdef J9VM_OPT_DEPRECATED_METHODS
	/* Prevent this thread from processing further stop requests */
	omrthread_monitor_enter(vmThread->publicFlagsMutex);
	clearEventFlag(vmThread, J9_PUBLIC_FLAGS_STOP);
	vmThread->stopThrowable = NULL;
	omrthread_monitor_exit(vmThread->publicFlagsMutex);
#endif

	/* Increment zombie thread counter - indicates threads which have notified java of their death, but have not deallocated their vmThread and exited their thread proc */

	omrthread_monitor_enter(vm->vmThreadListMutex);
	++(vm->zombieThreadCount);
	omrthread_monitor_exit(vm->vmThreadListMutex);

	/* Do the java dance to indicate thread death */
	acquireVMAccess(vmThread);
	cleanUpAttachedThread(vmThread);
	releaseVMAccess(vmThread);


#if defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390)
	/* Concurrent scavenge enabled and JIT loaded implies running on supported h/w.
	 * As such, per-thread deinitialization must occur
	 */
	if (vm->memoryManagerFunctions->j9gc_concurrent_scavenger_enabled(vm)
		&& (NULL != vm->jitConfig)
	) {
		if (0 == j9gs_deinitializeThread(vmThread)) {
			fatalError((JNIEnv *)vmThread, "Failed to deinitialize thread; please disable Concurrent Scavenge.\n");
		}
	}
#endif

	/* Deallocate the vmThread - if this thread was not forked by the VM, decrement the zombie counter now as the VM is not in control of the native thread */

	deallocateVMThread(vmThread, !forkedByVM, TRUE);
}

#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
static void
printCustomSpinOptions(void *element, void *userData)
{
	J9JavaVM *vm = (J9JavaVM *)userData;
	PORT_ACCESS_FROM_JAVAVM(vm);

	J9VMCustomSpinOptions *options = (J9VMCustomSpinOptions *)element;
	const J9ObjectMonitorCustomSpinOptions *const j9monitorOptions = &options->j9monitorOptions;
	const J9ThreadCustomSpinOptions *const j9threadOptions = &options->j9threadOptions;

	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "className=%s", options->className);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customSpin1=%zu", j9monitorOptions->thrMaxSpins1BeforeBlocking);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customSpin2=%zu", j9monitorOptions->thrMaxSpins2BeforeBlocking);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customYield=%zu", j9monitorOptions->thrMaxYieldsBeforeBlocking);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customTryEnterSpin1=%zu", j9monitorOptions->thrMaxTryEnterSpins1BeforeBlocking);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customTryEnterSpin2=%zu", j9monitorOptions->thrMaxTryEnterSpins2BeforeBlocking);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customTryEnterYield=%zu", j9monitorOptions->thrMaxTryEnterYieldsBeforeBlocking);
#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
#if defined(OMR_THR_THREE_TIER_LOCKING)
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customThreeTierSpinCount1=%zu", j9threadOptions->customThreeTierSpinCount1);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customThreeTierSpinCount2=%zu", j9threadOptions->customThreeTierSpinCount2);
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customThreeTierSpinCount3=%zu", j9threadOptions->customThreeTierSpinCount3);
#endif /* OMR_THR_THREE_TIER_LOCKING */
#if defined(OMR_THR_ADAPTIVE_SPIN)
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE_EXTRA "customAdaptSpin=%zu", j9threadOptions->customAdaptSpin);
#endif /* OMR_THR_ADAPTIVE_SPIN */
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */
}
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

/**
 * Initializes VM thread options and parses suboptions of -Xthr:
 *
 * Note that omrthread defaults are set at omrthread_init(), and not in
 * this function.
 *
 * @param[in] vm JavaVM.
 * @param[in] optArg Suboption string of format subopt[=val][,[subopt[=val]]...
 * @returns JNI error code
 * @retval JNI_OK success
 * @retval JNI_EINVAL unrecognized option
 */
jint
threadParseArguments(J9JavaVM *vm, char *optArg)
{
	char *scan_start;
	char *scan_limit;
	int dumpInfo = 0;
	UDATA cpus = 0;
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
	BOOLEAN customSpinOptionsParsed = FALSE;
#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
#if defined(OMR_THR_ADAPTIVE_SPIN)
	BOOLEAN customAdaptSpinEnabled = FALSE;
#endif /* OMR_THR_ADAPTIVE_SPIN */
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */
	PORT_ACCESS_FROM_JAVAVM(vm);

	cpus = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);

	/* initialize defaults, first */
	vm->thrMaxYieldsBeforeBlocking = 45;
	vm->thrMaxTryEnterYieldsBeforeBlocking = 45;
	vm->thrNestedSpinning = 1;
	vm->thrTryEnterNestedSpinning = 1;
	vm->thrDeflationPolicy = J9VM_DEFLATION_POLICY_ASAP;

	if (cpus > 1) {
#if (defined(LINUXPPC)) && !defined(J9VM_ENV_LITTLE_ENDIAN)
		vm->thrMaxSpins1BeforeBlocking = 151;
#elif defined(AIXPPC) || defined(LINUXPPC)
		vm->thrMaxSpins1BeforeBlocking = 96;
#else /* defined(AIXPPC) || defined(LINUXPPC) */
		vm->thrMaxSpins1BeforeBlocking = 256;
#endif /* defined(AIXPPC) || defined(LINUXPPC) */
		vm->thrMaxSpins2BeforeBlocking = 32;
		vm->thrMaxTryEnterSpins1BeforeBlocking = 256;
		vm->thrMaxTryEnterSpins2BeforeBlocking = 32;
	} else {
		/* In ObjectMonitor.cpp:objectMonitorEnterNonBlocking, we converted
		 * "goto statements" into "three nested for loops". Due to this change,
		 * we can no longer let spin counts to be 0. With spin counts set to
		 * zero, there will be no attempts to acquire a lock. To allow atleast
		 * one attempt to acquire a lock, minimum value of spin counts is set to 1.
		 */
		vm->thrMaxSpins1BeforeBlocking = 1;
		vm->thrMaxSpins2BeforeBlocking = 1;
		vm->thrMaxTryEnterSpins1BeforeBlocking = 1;
		vm->thrMaxTryEnterSpins2BeforeBlocking = 1;
	}

#if defined(J9ZOS390)
	{
		UDATA *gtw;

		/* Supply default thread weight, may be overridden below. */
		gtw = omrthread_global((char*)"thread_weight");
		*gtw = (UDATA)"medium";

		/* different defaults for z/OS */
		vm->thrNestedSpinning = 0;
		vm->thrTryEnterNestedSpinning = 0;
		vm->thrMaxYieldsBeforeBlocking = 128;
		vm->thrMaxTryEnterYieldsBeforeBlocking = 128;
		vm->thrMaxSpins2BeforeBlocking = 8;
		vm->thrMaxTryEnterSpins2BeforeBlocking = 8;

		/* In ObjectMonitor.cpp:objectMonitorEnterNonBlocking, we converted
		 * "goto statements" into "three nested for loops". Due to this change,
		 * we can no longer let spin counts to be 0. With spin counts set to
		 * zero, there will be no attempts to acquire a lock. To allow atleast
		 * one attempt to acquire a lock, minimum value of spin counts is set to 1.
		 */
		vm->thrMaxSpins1BeforeBlocking = 1;
		vm->thrMaxTryEnterSpins1BeforeBlocking = 1;
	}
#endif

#if defined(OMR_THR_YIELD_ALG)
	**(UDATA**)omrthread_global((char*)"yieldAlgorithm") = J9THREAD_LIB_YIELD_ALGORITHM_SCHED_YIELD;
	**(UDATA**)omrthread_global((char*)"yieldUsleepMultiplier") = 1;
#endif /* defined(OMR_THR_YIELD_ALG) */

#if defined(LINUX)
	/* Check the sched_compat_yield setting for the versions of the Completely Fair Scheduler (CFS) which
	 * have broken the thread_yield behavior. If running in CFS and sched_compat_yield=0, the CPU yielding
	 * behavior is moderated to act as though CFS is not enabled by increasing the yield count to 270 in
	 * the three-tier spinlock loops.
	 *
	 * sched_compat_yield=1 uses the aggressive CPU yielding behavior of some versions of the O(1)
	 * scheduler and uses the default yield counts (= 45).
	 *
	 * Newer Linux versions no longer support the sched_compat_yield flag since the thread_yield behavior
	 * is restored to something stable.
	 */
	if ('0' == j9util_sched_compat_yield_value(vm)) {
#if defined(OMR_THR_YIELD_ALG)
		**(UDATA**)omrthread_global((char*)"yieldAlgorithm") = J9THREAD_LIB_YIELD_ALGORITHM_INCREASING_USLEEP;
#if defined(OMR_THR_THREE_TIER_LOCKING)
		**(UDATA **)omrthread_global((char*)"defaultMonitorSpinCount3") = 270;
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
		vm->thrMaxYieldsBeforeBlocking = 270;
		vm->thrMaxTryEnterYieldsBeforeBlocking = 270;
#endif /* defined(OMR_THR_YIELD_ALG) */
	}
#endif /* defined(LINUX) */

	/* experimental options to force stacks to be relatively misaligned */
	vm->thrStaggerStep = 32;
	vm->thrStaggerMax = 0;
	vm->thrStagger = 0;

#if defined(J9VM_GC_REALTIME)
	/* If we aren't realtime, we can still be vanilla metronome (formerly known as SoftRT) and that means we still need to set these values but they are different */
	vm->priorityPosixSignalDispatch = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityPosixSignalDispatchNH = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityAsyncEventDispatch = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityAsyncEventDispatchNH = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityTimerDispatch = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityTimerDispatchNH = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityMetronomeAlarm = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityMetronomeTrace = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MIN) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityJitSample = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityJitCompile = PRIORITY_INDICATOR_VALUE(J9THREAD_PRIORITY_MAX) + PRIORITY_INDICATOR_ADJUSTED_TYPE(PRIORITY_INDICATOR_J9THREAD_PRIORITY);
	vm->priorityRealtimePriorityShift = 0;
#endif

#if defined(OMR_THR_ADAPTIVE_SPIN)
	*(UDATA*)omrthread_global((char*)"adaptSpinHoldtimeEnable")=1;
	*(UDATA*)omrthread_global((char*)"adaptSpinSlowPercentEnable")=1;
	**(UDATA**)omrthread_global((char*)"adaptSpinHoldtime")=1000000;
	**(UDATA**)omrthread_global((char*)"adaptSpinSlowPercent")=10;
	**(UDATA**)omrthread_global((char*)"adaptSpinSampleThreshold")=1000;
	**(UDATA**)omrthread_global((char*)"adaptSpinSampleStopCount")=10;
	**(UDATA**)omrthread_global((char*)"adaptSpinSampleCountStopRatio")=150;
	omrthread_lib_set_flags(J9THREAD_LIB_FLAG_ADAPTIVE_SPIN_KEEP_SAMPLING);
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
	/* Handle allocation and initialization of JLM tracing data structures for class-specific spin parameters */
	*(UDATA*)omrthread_global((char*)"customAdaptSpinEnabled") = customAdaptSpinEnabled;
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */
#endif /* OMR_THR_ADAPTIVE_SPIN */

#if defined(OMR_THR_THREE_TIER_LOCKING)
	omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_SECONDARY_SPIN_OBJECT_MONITORS_ENABLED | J9THREAD_LIB_FLAG_FAST_NOTIFY);
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	{
		UDATA maxSpinThreadsLocal = 0;
		if (cpus < OMRTHREAD_MINIMUM_SPIN_THREADS) {
			maxSpinThreadsLocal = OMRTHREAD_MINIMUM_SPIN_THREADS;
		} else if (cpus <= 4) {
			maxSpinThreadsLocal = cpus;
		} else if (cpus <= 16) {
			maxSpinThreadsLocal = cpus/2;
		} else if (cpus <= 64) {
			maxSpinThreadsLocal = cpus/3;
		} else {
			maxSpinThreadsLocal = cpus/4;
		}
		**(UDATA**)omrthread_global((char*)"maxSpinThreads") = maxSpinThreadsLocal;
	}
	**(UDATA**)omrthread_global((char*)"maxWakeThreads") = OMRTHREAD_MINIMUM_WAKE_THREADS;
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

	/* parse arguments */
	if (optArg == NULL) {
		return JNI_OK;
	}

	scan_start = optArg;
	scan_limit = optArg + strlen(optArg);
	while (scan_start < scan_limit) {

		/* ignore separators */
		try_scan(&scan_start, ",");


#if defined(J9VM_GC_REALTIME)
		{
			char *oldScanStart = scan_start;
			/* priorities will be determined algorithmically */
			if (try_scan(&scan_start, "spreadPrios")) {
				if (omrthread_set_priority_spread()) {
					scan_start = oldScanStart;
					goto _error;
				}
				continue;
			}
		}
#endif /* defined(J9VM_GC_REALTIME) */

		if (try_scan(&scan_start, "what")) {
			dumpInfo = 1;
			continue;
		}

		if (try_scan(&scan_start, "spin1=")) {
			if (scan_udata(&scan_start, &vm->thrMaxSpins1BeforeBlocking)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "spin2=")) {
			if (scan_udata(&scan_start, &vm->thrMaxSpins2BeforeBlocking)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "yield=")) {
			if (scan_udata(&scan_start, &vm->thrMaxYieldsBeforeBlocking)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "tryEnterSpin1=")) {
			if (scan_udata(&scan_start, &vm->thrMaxTryEnterSpins1BeforeBlocking)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "tryEnterSpin2=")) {
			if (scan_udata(&scan_start, &vm->thrMaxTryEnterSpins2BeforeBlocking)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "tryEnterYield=")) {
			if (scan_udata(&scan_start, &vm->thrMaxTryEnterYieldsBeforeBlocking)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "nestedSpinning")) {
			vm->thrNestedSpinning = 1;
			continue;
		}

		if (try_scan(&scan_start, "noNestedSpinning")) {
			vm->thrNestedSpinning = 0;
			continue;
		}

		if (try_scan(&scan_start, "tryEnterNestedSpinning")) {
			vm->thrTryEnterNestedSpinning = 1;
			continue;
		}

		if (try_scan(&scan_start, "noTryEnterNestedSpinning")) {
			vm->thrTryEnterNestedSpinning = 0;
			continue;
		}


		if (try_scan(&scan_start, "staggerStep=")) {
			if (scan_udata(&scan_start, &vm->thrStaggerStep)) {
				goto _error;
			}
			if (vm->thrStaggerStep & (sizeof(UDATA) - 1)) {
				vm->thrStaggerStep += (sizeof(UDATA) - (vm->thrStaggerStep & (sizeof(UDATA) - 1)));
			}
			continue;
		}

		if (try_scan(&scan_start, "staggerMax=")) {
			if (scan_udata(&scan_start, &vm->thrStaggerMax)) {
				goto _error;
			}
			continue;
		}

		if (try_scan(&scan_start, "noPriorities")) {
			vm->runtimeFlags |= J9_RUNTIME_NO_PRIORITIES;
			continue;
		}

#if !defined(WIN32) && defined(OMR_NOTIFY_POLICY_CONTROL)
		if (try_scan(&scan_start, "notifyPolicy=")) {
			char *oldScanStart = scan_start;
			char *notifyPolicy = scan_to_delim(PORTLIB, &scan_start, ',');
			if (NULL != notifyPolicy) {
				if (0 == strcmp(notifyPolicy, "signal")) {
					omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_NOTIFY_POLICY_BROADCAST);
				} else if (0 == strcmp(notifyPolicy, "broadcast")) {
					omrthread_lib_set_flags(J9THREAD_LIB_FLAG_NOTIFY_POLICY_BROADCAST);
				} else {
					/* restore for better error message */
					scan_start = oldScanStart;
					j9mem_free_memory(notifyPolicy);
					goto _error;
				}
				j9mem_free_memory(notifyPolicy);
			} else {
				goto _error;
			}
			continue;
		}
#endif /* !defined(WIN32) && defined(OMR_NOTIFY_POLICY_CONTROL) */

#if defined(OMR_THR_THREE_TIER_LOCKING)
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
		if (try_scan(&scan_start, "maxSpinThreads=")) {
			UDATA maxSpinThreads = 0;
			if (scan_udata(&scan_start, &maxSpinThreads)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"maxSpinThreads") = maxSpinThreads;
			continue;
		}
		if (try_scan(&scan_start, "maxWakeThreads=")) {
			UDATA maxWakeThreads = 0;
			if (scan_udata(&scan_start, &maxWakeThreads)) {
				goto _error;
			}
			if (maxWakeThreads < OMRTHREAD_MINIMUM_WAKE_THREADS) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"maxWakeThreads") = maxWakeThreads;
			continue;
		}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
		if (try_scan(&scan_start, "threeTierSpinCount1=")) {
				UDATA spinCount;
				if (scan_udata(&scan_start, &spinCount)) {
					goto _error;
				}
				if (0 == spinCount) {
					goto _error;
				}
				**(UDATA**)omrthread_global((char*)"defaultMonitorSpinCount1") = spinCount;
				continue;
		}
		if (try_scan(&scan_start, "threeTierSpinCount2=")) {
				UDATA spinCount;
				if (scan_udata(&scan_start, &spinCount)) {
					goto _error;
				}
				if (0 == spinCount) {
					goto _error;
				}
				**(UDATA**)omrthread_global((char*)"defaultMonitorSpinCount2") = spinCount;
				continue;
		}
		if (try_scan(&scan_start, "threeTierSpinCount3=")) {
				UDATA spinCount;
				if (scan_udata(&scan_start, &spinCount)) {
					goto _error;
				}
				if (0 == spinCount) {
					goto _error;
				}
				**(UDATA**)omrthread_global((char*)"defaultMonitorSpinCount3") = spinCount;
				continue;
		}
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

		if (try_scan(&scan_start, "minimizeUserCPU")) {
			initMinCPUSpinCounts(vm);
#ifdef OMR_THR_ADAPTIVE_SPIN
			*(UDATA*)omrthread_global((char*)"adaptSpinHoldtimeEnable") = 0;
			*(UDATA*)omrthread_global((char*)"adaptSpinSlowPercentEnable") = 0;
#endif

#if defined(OMR_THR_YIELD_ALG)
			**(UDATA**)omrthread_global((char*)"yieldAlgorithm") = J9THREAD_LIB_YIELD_ALGORITHM_SCHED_YIELD;
#endif /* defined(OMR_THR_YIELD_ALG) */
			continue;
		}

#ifdef OMR_THR_JLM_HOLD_TIMES
		if (try_scan(&scan_start, "clockSkewHi=")) {
				/* upper 32 bits of 64 bit clock */
				UDATA clockSkewHi;
				if (scan_hex(&scan_start, &clockSkewHi)) {
					goto _error;
				}
				*omrthread_global((char*)"clockSkewHi") = clockSkewHi;
				continue;
		}
#endif

		if (try_scan(&scan_start, "deflationPolicy=")) {
			char *oldScanStart = scan_start;
			char *policy = scan_to_delim(PORTLIB, &scan_start, ',');
			if (NULL != policy) {
				if (0 == strcmp(policy, "never")) {
					vm->thrDeflationPolicy = J9VM_DEFLATION_POLICY_NEVER;
				} else if (0 == strcmp(policy, "asap")) {
					vm->thrDeflationPolicy = J9VM_DEFLATION_POLICY_ASAP;
				}
#ifdef J9VM_THR_SMART_DEFLATION
				else if (0 == strcmp(policy,"smart")) {
					vm->thrDeflationPolicy = J9VM_DEFLATION_POLICY_SMART;
				}
#endif
				else {
					/* restore for better error message */
					scan_start = oldScanStart;
					j9mem_free_memory(policy);
					goto _error;
				}
				j9mem_free_memory(policy);
			} else {
				goto _error;
			}
			continue;
		}

#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
		if (try_scan(&scan_start, "customSpinOptions=")) {
			vm->customSpinOptions = pool_new(sizeof(J9VMCustomSpinOptions), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
			if (NULL == vm->customSpinOptions) {
				goto _error;
			}

			do {
				J9VMCustomSpinOptions *option = (J9VMCustomSpinOptions*) pool_newElement(vm->customSpinOptions);
				J9ObjectMonitorCustomSpinOptions *j9monitorOptions = &option->j9monitorOptions;
				J9ThreadCustomSpinOptions *j9threadOptions = &option->j9threadOptions;

				if (NULL == option) {
					goto _error;
				}

				option->className = scan_to_delim(vm->portLibrary, &scan_start, ':');
				if(NULL == option->className) {
					goto _error;
				}

				if (scan_udata(&scan_start, &j9monitorOptions->thrMaxSpins1BeforeBlocking)) {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (scan_udata(&scan_start, &j9monitorOptions->thrMaxSpins2BeforeBlocking)) {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (scan_udata(&scan_start, &j9monitorOptions->thrMaxYieldsBeforeBlocking)) {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (scan_udata(&scan_start, &j9monitorOptions->thrMaxTryEnterSpins1BeforeBlocking)) {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (scan_udata(&scan_start, &j9monitorOptions->thrMaxTryEnterSpins2BeforeBlocking)) {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (scan_udata(&scan_start, &j9monitorOptions->thrMaxTryEnterYieldsBeforeBlocking)) {
					goto _error;
				}

#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
#if defined(OMR_THR_THREE_TIER_LOCKING)
				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (0 == scan_udata(&scan_start, &j9threadOptions->customThreeTierSpinCount1)) {
					if (0 == j9threadOptions->customThreeTierSpinCount1) {
						goto _error;
					}
				} else {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (0 == scan_udata(&scan_start, &j9threadOptions->customThreeTierSpinCount2)) {
					if (0 == j9threadOptions->customThreeTierSpinCount2) {
						goto _error;
					}
				} else {
					goto _error;
				}

				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				if (0 == scan_udata(&scan_start, &j9threadOptions->customThreeTierSpinCount3)) {
					if (0 == j9threadOptions->customThreeTierSpinCount3) {
						goto _error;
					}
				} else {
					goto _error;
				}
#endif /* OMR_THR_THREE_TIER_LOCKING */
#if defined(OMR_THR_ADAPTIVE_SPIN)
				if (!try_scan(&scan_start, ":")) {
					goto _error;
				}

				{
					UDATA customAdaptSpin = 0;
					if (0 == scan_udata(&scan_start, &customAdaptSpin)) {
						if (0 == customAdaptSpin) {
							j9threadOptions->customAdaptSpin = 0;
						} else if (1 == customAdaptSpin) {
							j9threadOptions->customAdaptSpin = 1;
							customAdaptSpinEnabled = TRUE;
						} else {
							goto _error;
						}
					} else {
						goto _error;
					}
				}
#endif /* OMR_THR_ADAPTIVE_SPIN */
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */

				if (!try_scan(&scan_start, ":")) {
					customSpinOptionsParsed = TRUE;
				}
			} while (FALSE == customSpinOptionsParsed);

#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
#if defined(OMR_THR_ADAPTIVE_SPIN)
			*(UDATA*)omrthread_global((char*)"customAdaptSpinEnabled") = customAdaptSpinEnabled;
#endif /* OMR_THR_ADAPTIVE_SPIN */
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */
			continue;
		}
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

#ifdef OMR_THR_ADAPTIVE_SPIN
		if (try_scan(&scan_start, "adaptSpinHoldtime=")) {
			UDATA adaptSpinHoldtime;
			if (scan_udata(&scan_start, &adaptSpinHoldtime)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"adaptSpinHoldtime") = adaptSpinHoldtime;
			*(UDATA*)omrthread_global((char*)"adaptSpinHoldtimeEnable") = (0 != adaptSpinHoldtime);
			continue;
		}

		if (try_scan(&scan_start, "adaptSpinSlowPercent=")) {
			UDATA adaptSpinSlowPercent;
			if (scan_udata(&scan_start, &adaptSpinSlowPercent)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"adaptSpinSlowPercent") = adaptSpinSlowPercent;
			*(UDATA*)omrthread_global((char*)"adaptSpinSlowPercentEnable") = (0 != adaptSpinSlowPercent);
			continue;
		}

		if (try_scan(&scan_start, "adaptSpinSampleThreshold=")) {
			UDATA adaptSpinSampleThreshold;
			if (scan_udata(&scan_start, &adaptSpinSampleThreshold)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"adaptSpinSampleThreshold") = adaptSpinSampleThreshold;
			continue;
		}

		if (try_scan(&scan_start, "adaptSpinSampleStopCount=")) {
			UDATA adaptSpinSampleStopCount;
			if (scan_udata(&scan_start, &adaptSpinSampleStopCount)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"adaptSpinSampleStopCount") = adaptSpinSampleStopCount;
			continue;
		}

		if (try_scan(&scan_start, "adaptSpinSampleCountStopRatio=")) {
			UDATA adaptSpinSampleCountStopRatio;
			if (scan_udata(&scan_start, &adaptSpinSampleCountStopRatio)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"adaptSpinSampleCountStopRatio") = adaptSpinSampleCountStopRatio;
			continue;
		}

		if (try_scan(&scan_start, "adaptSpinKeepSampling")) {
			omrthread_lib_set_flags(J9THREAD_LIB_FLAG_ADAPTIVE_SPIN_KEEP_SAMPLING);
			continue;
		}

		if (try_scan(&scan_start, "adaptSpinNoKeepSampling")) {
			omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_ADAPTIVE_SPIN_KEEP_SAMPLING);
			continue;
		}

		if (try_scan(&scan_start, "adaptSpin")) {
			*(UDATA*)omrthread_global((char*)"adaptSpinHoldtimeEnable") = 1;
			*(UDATA*)omrthread_global((char*)"adaptSpinSlowPercentEnable") = 1;
			continue;
		}

		if (try_scan(&scan_start, "noAdaptSpin")) {
			*(UDATA*)omrthread_global((char*)"adaptSpinHoldtimeEnable") = 0;
			*(UDATA*)omrthread_global((char*)"adaptSpinSlowPercentEnable") = 0;
			continue;
		}
#endif

#if defined(OMR_THR_THREE_TIER_LOCKING)
		if (try_scan(&scan_start, "secondarySpinForObjectMonitors")) {
			omrthread_lib_set_flags(J9THREAD_LIB_FLAG_SECONDARY_SPIN_OBJECT_MONITORS_ENABLED);
			continue;
		}

		if (try_scan(&scan_start, "noSecondarySpinForObjectMonitors")) {
			omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_SECONDARY_SPIN_OBJECT_MONITORS_ENABLED);
			continue;
		}

		if (try_scan(&scan_start, "fastNotify")) {
			omrthread_lib_set_flags(J9THREAD_LIB_FLAG_FAST_NOTIFY);
			continue;
		}

		if (try_scan(&scan_start, "noFastNotify")) {
			omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_FAST_NOTIFY);
			continue;
		}
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

#if defined(OMR_THR_YIELD_ALG)
		if (try_scan(&scan_start, "yieldAlgorithm=")) {
			UDATA yieldAlgorithm;
			if (scan_udata(&scan_start, &yieldAlgorithm)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"yieldAlgorithm") = yieldAlgorithm;
			continue;
		}

		if (try_scan(&scan_start, "yieldUsleepMultiplier=")) {
			UDATA yieldUsleepMultiplier;
			if (scan_udata(&scan_start, &yieldUsleepMultiplier)) {
				goto _error;
			}
			**(UDATA**)omrthread_global((char*)"yieldUsleepMultiplier") = yieldUsleepMultiplier;
			continue;
		}

		/* scan for -Xthr:cfsYield again so the argument is consumed */
		if (try_scan(&scan_start, "cfsYield")) {
			/* Use yield algorithm for CFS systems.
			 * We set the algorithm again here so that the last argument wins.
			 */
			**(UDATA**)omrthread_global((char*)"yieldAlgorithm") = J9THREAD_LIB_YIELD_ALGORITHM_INCREASING_USLEEP;
			continue;
		}

		if (try_scan(&scan_start, "noCfsYield")) {
			/* use pthread yield */
			**(UDATA**)omrthread_global((char*)"yieldAlgorithm") = J9THREAD_LIB_YIELD_ALGORITHM_SCHED_YIELD;
			initMinCPUSpinCounts(vm);
			continue;
		}

#endif /* defined(OMR_THR_YIELD_ALG) */


#if defined(J9ZOS390)
		if (try_scan(&scan_start, "tw=")) {
			char *old_scan_start = scan_start; /* for error handling */
			char *tw = scan_to_delim(PORTLIB, &scan_start, ',');
			UDATA *gtw = omrthread_global((char*)"thread_weight");
			if (tw && !strcmp(tw, "HEAVY")) {
				tw = "heavy";
			} else if (tw && (!strcmp(tw, "MEDIUM") || !strcmp(tw, "NATIVE") || !strcmp(tw, "native") || !strcmp(tw, ""))) {
				tw = "medium";
			}
			*gtw = (UDATA)tw;
			if (tw && !strcmp(tw, "heavy")) {
				continue;
			}
			if (tw && !strcmp(tw, "medium")) {
				continue;
			}
			*gtw = 0;
			scan_start = old_scan_start;
			goto _error;
		}
#endif

#if defined(AIXPPC)
		/* Disable thread priorities and policies */
		if (try_scan(&scan_start, "noThreadScheduling")) {
			omrthread_lib_set_flags(J9THREAD_LIB_FLAG_NO_SCHEDULING);
			continue;
		}
#endif

		if (try_scan(&scan_start, "destroyMutexOnMonitorFree")) {
			omrthread_lib_set_flags(J9THREAD_LIB_FLAG_DESTROY_MUTEX_ON_MONITOR_FREE);
			continue;
		}

		if (try_scan(&scan_start, "noDestroyMutexOnMonitorFree")) {
			omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_DESTROY_MUTEX_ON_MONITOR_FREE);
			continue;
		}

		/* Couldn't find a match for arguments */
_error:
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_UNRECOGNIZED_XTHR_OPTION, scan_start);
		return JNI_EINVAL;
	}

	if (dumpInfo) {
		dumpThreadingInfo(vm);
	}

	return JNI_OK;

}

static void
initMinCPUSpinCounts(J9JavaVM *vm)
{
#if defined(OMR_THR_THREE_TIER_LOCKING)
	**(UDATA **)omrthread_global((char*)"defaultMonitorSpinCount1") = 1;
	**(UDATA **)omrthread_global((char*)"defaultMonitorSpinCount2") = 1;
	**(UDATA **)omrthread_global((char*)"defaultMonitorSpinCount3") = 1;
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

	/* In ObjectMonitor.cpp:objectMonitorEnterNonBlocking, we converted
	 * "goto statements" into "three nested for loops". Due to this change,
	 * we can no longer let spin counts to be 0. With spin counts set to
	 * zero, there will be no attempts to acquire a lock. To allow atleast
	 * one attempt to acquire a lock, minimum value of spin counts is set to 1.
	 */
	vm->thrMaxSpins1BeforeBlocking = 1;
	vm->thrMaxSpins2BeforeBlocking = 1;
	vm->thrMaxYieldsBeforeBlocking = 1;

	vm->thrMaxTryEnterSpins1BeforeBlocking = 1;
	vm->thrMaxTryEnterSpins2BeforeBlocking = 1;
	vm->thrMaxTryEnterYieldsBeforeBlocking = 1;
}

static void
dumpThreadingInfo(J9JavaVM* jvm)
{
	PORT_ACCESS_FROM_JAVAVM(jvm);
#ifdef J9ZOS390
	int envbuflen = 0;
	char *envbuf = NULL;
#endif

	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_XTHR_WHAT_TITLE);
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_XTHR_WHAT_TITLE_SEP);

#if defined(OMR_THR_THREE_TIER_LOCKING)
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_THREE_TIER_SUPPORTED);
#else /* defined(OMR_THR_THREE_TIER_LOCKING) */
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_THREE_TIER_NOT_SUPPORTED);
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

#ifdef OMR_THR_JLM
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_JLM_SUPPORTED);
#ifdef OMR_THR_JLM_HOLD_TIMES
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_JLM_HOLD_TIMES_SUPPORTED);
#else
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_JLM_HOLD_TIMES_NOT_SUPPORTED);
#endif
#else
	j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_JLM_NOT_SUPPORTED);
#endif

#ifdef OMR_THR_ADAPTIVE_SPIN
	j9nls_printf(PORTLIB, J9NLS_INFO,J9NLS_VM_ADAPTIVE_SPIN_SUPPORTED);
#else
	j9nls_printf(PORTLIB, J9NLS_INFO,J9NLS_VM_ADAPTIVE_SPIN_NOT_SUPPORTED);
#endif

#ifdef J9ZOS390
	envbuflen = j9sysinfo_get_env("_EDC_PTHREAD_YIELD", NULL, 0);
	if (envbuflen > 0) {
		envbuf = (char*)j9mem_allocate_memory(envbuflen + 1, OMRMEM_CATEGORY_VM);
		if (envbuf != NULL) {
			if (j9sysinfo_get_env("_EDC_PTHREAD_YIELD", envbuf, envbuflen) != 0) {
				envbuf[0] = '\0';
			} else {
				envbuf[envbuflen] = '\0';
			}
		}
	}
	j9tty_printf(PORTLIB, "_EDC_PTHREAD_YIELD=%s\n", envbuf? envbuf: "");
	j9mem_free_memory(envbuf);
#endif


	j9tty_printf(PORTLIB, "-Xthr:\n");
	j9tty_printf(PORTLIB, LEADING_SPACE "staggerMax=%zu,\n", jvm->thrStaggerMax);
	j9tty_printf(PORTLIB, LEADING_SPACE "staggerStep=%zu,\n", jvm->thrStaggerStep);
	j9tty_printf(PORTLIB, LEADING_SPACE "spin1=%zu,\n", jvm->thrMaxSpins1BeforeBlocking);
	j9tty_printf(PORTLIB, LEADING_SPACE "spin2=%zu,\n", jvm->thrMaxSpins2BeforeBlocking);
	j9tty_printf(PORTLIB, LEADING_SPACE "yield=%zu,\n", jvm->thrMaxYieldsBeforeBlocking);
	j9tty_printf(PORTLIB, LEADING_SPACE "tryEnterSpin1=%zu,\n", jvm->thrMaxTryEnterSpins1BeforeBlocking);
	j9tty_printf(PORTLIB, LEADING_SPACE "tryEnterSpin2=%zu,\n", jvm->thrMaxTryEnterSpins2BeforeBlocking);
	j9tty_printf(PORTLIB, LEADING_SPACE "tryEnterYield=%zu,\n", jvm->thrMaxTryEnterYieldsBeforeBlocking);
	j9tty_printf(PORTLIB, LEADING_SPACE "%sestedSpinning,\n", (jvm->thrNestedSpinning) ? "n" : "noN");
	j9tty_printf(PORTLIB, LEADING_SPACE "%sryEnterNestedSpinning,\n", (jvm->thrTryEnterNestedSpinning) ? "t" : "noT");
	j9tty_printf(PORTLIB, LEADING_SPACE "%sestroyMutexOnMonitorFree,\n",
		J9_ARE_ALL_BITS_SET(omrthread_lib_get_flags(), J9THREAD_LIB_FLAG_DESTROY_MUTEX_ON_MONITOR_FREE) ? "d" : "noD");
#if !defined(WIN32) && defined(OMR_NOTIFY_POLICY_CONTROL)
	j9tty_printf(PORTLIB, LEADING_SPACE "notifyPolicy=%s,\n",
		J9_ARE_ALL_BITS_SET(omrthread_lib_get_flags(), J9THREAD_LIB_FLAG_NOTIFY_POLICY_BROADCAST) ? "broadcast" : "signal");
#endif /* !defined(WIN32) && defined(OMR_NOTIFY_POLICY_CONTROL) */
	j9tty_printf(PORTLIB, LEADING_SPACE "deflationPolicy=%s", (jvm->thrDeflationPolicy == J9VM_DEFLATION_POLICY_ASAP) ? "asap" :
		(jvm->thrDeflationPolicy == J9VM_DEFLATION_POLICY_NEVER) ? "never" : "smart");
#if defined(OMR_THR_THREE_TIER_LOCKING)
	j9tty_printf(PORTLIB, ",\n");
	j9tty_printf(PORTLIB, LEADING_SPACE "threeTierSpinCount1=%zu,\n", **(UDATA**)omrthread_global((char*)"defaultMonitorSpinCount1"));
	j9tty_printf(PORTLIB, LEADING_SPACE "threeTierSpinCount2=%zu,\n", **(UDATA**)omrthread_global((char*)"defaultMonitorSpinCount2"));
	j9tty_printf(PORTLIB, LEADING_SPACE "threeTierSpinCount3=%zu,\n", **(UDATA**)omrthread_global((char*)"defaultMonitorSpinCount3"));
	j9tty_printf(PORTLIB, LEADING_SPACE "%sastNotify", J9_ARE_ALL_BITS_SET(omrthread_lib_get_flags(), J9THREAD_LIB_FLAG_FAST_NOTIFY) ? "f" : "noF");
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "maxSpinThreads=%zu,\n", **(UDATA**)omrthread_global((char*)"maxSpinThreads"));
	j9tty_printf(PORTLIB, LEADING_SPACE "maxWakeThreads=%zu", **(UDATA**)omrthread_global((char*)"maxWakeThreads"));
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
#ifdef OMR_THR_JLM_HOLD_TIMES
	j9tty_printf(PORTLIB, ",\n");
	j9tty_printf(PORTLIB, LEADING_SPACE "clockSkewHi=0x%zx", *omrthread_global((char*)"clockSkewHi") );
#endif
#ifdef J9ZOS390
	{
		char* pThreadWeight = *((char**)omrthread_global((char*)"thread_weight"));
		if (NULL != pThreadWeight) {
			j9tty_printf(PORTLIB, ",\n");
			j9tty_printf(PORTLIB, LEADING_SPACE "tw=%s", pThreadWeight);
		}
	}
#endif

	if (J9_ARE_ALL_BITS_SET(omrthread_lib_get_flags(), J9THREAD_LIB_FLAG_NO_SCHEDULING)) {
		j9tty_printf(PORTLIB, ",\n");
		j9tty_printf(PORTLIB, LEADING_SPACE "noThreadScheduling");
	}

#if defined(OMR_THR_THREE_TIER_LOCKING)
	j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "%secondarySpinForObjectMonitors",
		J9_ARE_ALL_BITS_SET(omrthread_lib_get_flags(), J9THREAD_LIB_FLAG_SECONDARY_SPIN_OBJECT_MONITORS_ENABLED) ? "s" : "noS");
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

#ifdef OMR_THR_ADAPTIVE_SPIN
	if ((0 != *(UDATA*)omrthread_global((char*)"adaptSpinHoldtimeEnable")) || (0 != *(UDATA*)omrthread_global((char*)"adaptSpinSlowPercentEnable"))) {
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpin");
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpinHoldtime=%zu", **(UDATA**)omrthread_global((char*)"adaptSpinHoldtime"));
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpinSlowPercent=%zu", **(UDATA**)omrthread_global((char*)"adaptSpinSlowPercent"));
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpinSampleThreshold=%zu",**(UDATA**)omrthread_global((char*)"adaptSpinSampleThreshold"));
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpinSampleStopCount=%zu",**(UDATA**)omrthread_global((char*)"adaptSpinSampleStopCount"));
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpinSampleCountStopRatio=%zu",**(UDATA**)omrthread_global((char*)"adaptSpinSampleCountStopRatio"));
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "adaptSpin%sKeepSampling",
			J9_ARE_ALL_BITS_SET(omrthread_lib_get_flags(), J9THREAD_LIB_FLAG_ADAPTIVE_SPIN_KEEP_SAMPLING) ? "" : "No");
	} else {
		j9tty_printf(PORTLIB, ",\n" LEADING_SPACE "noAdaptSpin");
	}
#endif

#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
	if (NULL != jvm->customSpinOptions) {
		pool_do(jvm->customSpinOptions, printCustomSpinOptions, (void *)jvm);
	}
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

	j9tty_printf(PORTLIB, "\n");
}



J9JavaStack *
allocateJavaStack(J9JavaVM * vm, UDATA stackSize, J9JavaStack * previousStack)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JavaStack * stack;
	UDATA mallocSize;

	/* Allocate the stack, adding the header and overflow area size.
	 * Add one slot for possible double-slot alignment.
	 * If stagger is in use, add the maximum stagger value to account for that alignment.
	 */

	mallocSize = J9_STACK_OVERFLOW_AND_HEADER_SIZE + (stackSize + sizeof(UDATA)) + vm->thrStaggerMax;
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		stack = (J9JavaStack*)j9mem_allocate_memory32(mallocSize, OMRMEM_CATEGORY_THREADS_RUNTIME_STACK);
	} else {
		stack = (J9JavaStack*)j9mem_allocate_memory(mallocSize, OMRMEM_CATEGORY_THREADS_RUNTIME_STACK);
	}
	if (stack != NULL) {
		/* for hyperthreading platforms, make sure that stacks are relatively misaligned */
		UDATA end = ((UDATA) stack) + J9_STACK_OVERFLOW_AND_HEADER_SIZE + stackSize;
		UDATA stagger = vm->thrStagger;
		stagger += vm->thrStaggerStep;
		vm->thrStagger = stagger = stagger >= vm->thrStaggerMax ? 0 : stagger;
		if (vm->thrStaggerMax != 0) {
			end += vm->thrStaggerMax - ((UDATA)end + stagger) % vm->thrStaggerMax;
		}
		/* Ensure that the stack ends on a double-slot boundary */
		if (J9_ARE_ANY_BITS_SET(end, sizeof(UDATA))) {
			end += sizeof(UDATA);
		}
#if 0
		j9tty_printf(PORTLIB, "Allocated stack ending at 16r%p (stagger = %d, alignment = %d)\n",
			end,
			stagger,
			vm->thrStaggerMax ? end % vm->thrStaggerMax : 0);
#endif

		stack->end = (UDATA*)end;
		stack->size = stackSize;
		stack->previous = previousStack;
		stack->firstReferenceFrame = 0;

#if JAVA_SPEC_VERSION >= 19
		stack->isVirtual = FALSE;
#endif /* JAVA_SPEC_VERSION >= 19 */

		/* If this is a profiling VM, or verbose:stack is enabled, paint the stack with a distinctive pattern so we can
			determine how much has been used */

#if defined (J9VM_INTERP_VERBOSE) || defined (J9VM_PROF_EVENT_REPORTING)
		if (vm->runtimeFlags & J9_RUNTIME_PAINT_STACK) {
			UDATA * currentSlot = (UDATA *) (stack + 1);

			while (currentSlot != stack->end)
				*currentSlot++ = J9_RUNTIME_STACK_FILL;
		}
#endif
	}

	return stack;
}


void
freeJavaStack(J9JavaVM * vm, J9JavaStack * stack)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		j9mem_free_memory32(stack);
	} else {
		j9mem_free_memory(stack);
	}
}


void
fatalRecursiveStackOverflow(J9VMThread *currentThread)
{
	BOOLEAN fatalRecursiveStackOverflowDetected = FALSE;
	Assert_VM_true(fatalRecursiveStackOverflowDetected);
}

static UDATA printMethodInfo(J9VMThread *currentThread , J9StackWalkState *state)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9Method *method = state->method;
	J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(methodClass->romClass);
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8* methodName = J9ROMMETHOD_NAME(romMethod);
	J9UTF8* sig = J9ROMMETHOD_SIGNATURE(romMethod);
	IDATA tracefd = (IDATA) state->userData1;
	char buf[1024] = "";
	char *cursor = buf;
	char *end = buf + sizeof(buf);
	PORT_ACCESS_FROM_VMC(currentThread);

	cursor += j9str_printf(PORTLIB, cursor, end - cursor, "\tat %.*s.%.*s%.*s", J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));

	if (romMethod->modifiers & J9AccNative) {
	/*increment cursor here by the return of j9str_printf if it needs to be used further*/
		j9str_printf(PORTLIB, cursor, end - cursor, " (Native Method)");
	} else {
		UDATA offsetPC = state->bytecodePCOffset;
#ifdef J9VM_OPT_DEBUG_INFO_SERVER
		J9UTF8 *sourceFile = getSourceFileNameForROMClass(vm, methodClass->classLoader, methodClass->romClass);

		if (sourceFile) {
			IDATA lineNumber = getLineNumberForROMClass(vm, method, offsetPC);

			cursor += j9str_printf(PORTLIB, cursor, end - cursor, " (%.*s", J9UTF8_LENGTH(sourceFile), J9UTF8_DATA(sourceFile));
			if (lineNumber != -1) {
				cursor += j9str_printf(PORTLIB, cursor, end - cursor, ":%zu", lineNumber);
			}
			cursor += j9str_printf(PORTLIB, cursor, end - cursor, ")");
		} else
#endif
		{
			cursor += j9str_printf(PORTLIB, cursor, end - cursor, " (Bytecode PC: %zu)", offsetPC);
		}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
		if (state->jitInfo != NULL) {
		/*increment cursor here by the return of j9str_printf if it needs to be used further*/
			j9str_printf(PORTLIB, cursor, end - cursor, " (Compiled Code)");
		}
#endif
	}

	trace_printf(PORTLIB, tracefd, (char*)"%s\n", buf);

	return J9_STACKWALK_KEEP_ITERATING;
}

void printThreadInfo(J9JavaVM *vm, J9VMThread *self, char *toFile, BOOLEAN allThreads)
{
	J9VMThread *currentThread = NULL, *firstThread = NULL;
	IDATA tracefd = -1;
	char fileName[EsMaxPath];
	J9PortLibrary* privatePortLibrary = vm->portLibrary;
	int releaseAccess = 0;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	int exitVM = 0;
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	BOOLEAN exclusiveRequestedLocally = FALSE;

	if ( !vm->mainThread ) {
		/* No main thread, so not much we can do here */
		j9tty_err_printf(PORTLIB, "Thread info not available\n");
		return;
	}

	firstThread = currentThread = (self ? self : vm->mainThread);

	if(J9_XACCESS_NONE == vm->exclusiveAccessState) {
		if (NULL != self) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			if (self->inNative) {
				internalEnterVMFromJNI(self);
				exitVM = 1;
			} else
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			if ( 0 == (self->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) ) {
				internalAcquireVMAccess(self);
				releaseAccess = 1;
			}
			acquireExclusiveVMAccess(self);
		} else {
			acquireExclusiveVMAccessFromExternalThread(vm);
		}
		exclusiveRequestedLocally = TRUE;
	}

	if (toFile != NULL) {
		strcpy(fileName, toFile);
		if ((tracefd = j9file_open(fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666))==-1) {
			j9tty_err_printf(PORTLIB, "Error: Failed to open dump file %s.\nWill output to stderr instead:\n", fileName);
		}
	} else if (vm->sigquitToFileDir != NULL) {
		j9str_printf(PORTLIB, fileName, EsMaxPath, "%s%s%s%d%s",
							vm->sigquitToFileDir, DIR_SEPARATOR_STR, SIGQUIT_FILE_NAME, j9time_usec_clock(), SIGQUIT_FILE_EXT);
		if ((tracefd = j9file_open(fileName, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666))==-1) {
			j9tty_err_printf(PORTLIB, "Error: Failed to open trace file %s.\nWill output to stderr instead:\n", fileName);
		}
	}

	if (currentThread) {
		do {
			if (currentThread->threadObject) {
				char* threadName = getOMRVMThreadName(currentThread->omrVMThread);
				J9StackWalkState walkState;

				trace_printf(PORTLIB, tracefd, (char*)"Thread=%s (%p) Status=%s\n", threadName, currentThread->osThread, (char*)getJ9ThreadStatus(currentThread));

				releaseOMRVMThreadName(currentThread->omrVMThread);

				printJ9ThreadStatusMonitorInfo(currentThread, tracefd);

				walkState.walkThread = currentThread;
				walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
				walkState.skipCount = 0;
				walkState.userData1 = (void*)(UDATA)tracefd;
#if 0
				/* fake a reference for searching */
				printMethodInfo();
#endif
				walkState.frameWalkFunction =  printMethodInfo;

				vm->walkStackFrames(firstThread, &walkState);
			}

			if (allThreads) {
				trace_printf(PORTLIB, tracefd, (char*)"\n");
				currentThread = currentThread->linkNext;
			}
		} while (currentThread != firstThread);
	}

	if (tracefd != -1) {
		j9file_close(tracefd);
		j9tty_err_printf(PORTLIB, "Thread info written to: %s\n", fileName);
	}

	if(exclusiveRequestedLocally) {
		if (self) {
			releaseExclusiveVMAccess(self);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			if ( exitVM ) {
				internalExitVMToJNI(self);
			} else
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			if ( releaseAccess ) {
				internalReleaseVMAccess(self);
			}
		} else {
			releaseExclusiveVMAccessFromExternalThread(vm);
		}
	}
}

static char *
getJ9ThreadStatus(J9VMThread *vmThread)
{
	J9ThreadAbstractMonitor *blockingMonitor = NULL;
	J9VMThread *owner = NULL;
	UDATA count = 0;

	switch(getVMThreadRawState(vmThread, NULL, (omrthread_monitor_t *)&blockingMonitor, &owner, &count)) {
		case J9VMTHREAD_STATE_BLOCKED:
			if (blockingMonitor->flags & J9THREAD_MONITOR_INFLATED) {
				return (char*)"Blocked";
			} else {
				return (char*)"Blocked on flat lock";
			}
		case J9VMTHREAD_STATE_WAITING:
		case J9VMTHREAD_STATE_WAITING_TIMED:
			return (char*)"Waiting";
		case J9VMTHREAD_STATE_SLEEPING:
			return (char*)"Sleeping";
		case J9VMTHREAD_STATE_SUSPENDED:
			return (char*)"Suspended";
		case J9VMTHREAD_STATE_PARKED:
		case J9VMTHREAD_STATE_PARKED_TIMED:
			return (char*)"Parked";
	}
	return (char*)"Running";
}

static void
printJ9ThreadStatusMonitorInfo(J9VMThread *vmThread, IDATA tracefd)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9ThreadAbstractMonitor *blockingMonitor = NULL;
	J9VMThread *owner = NULL;
	char* ownerName = (char*)"";
	void* ownerPtr;
	UDATA count = 0;
	const char* name;

	(void)getVMThreadRawState(vmThread, NULL, (omrthread_monitor_t *)&blockingMonitor, &owner, &count);

	if (blockingMonitor == NULL) {
		return;
	}

	if ( (((J9ThreadAbstractMonitor*)blockingMonitor)->flags & J9THREAD_MONITOR_OBJECT) == J9THREAD_MONITOR_OBJECT ) {
		j9object_t object = (j9object_t)((J9ThreadAbstractMonitor*)blockingMonitor)->userData;
		J9ROMClass *romClass;
		J9UTF8* className;
		char* objType;
		if (J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, object)) {
			romClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, object)->romClass;
			objType = (char*)"Class";
		} else {
			romClass = J9OBJECT_CLAZZ(vmThread, object)->romClass;
			objType = (char*)"Object";
		}
		className = J9ROMCLASS_CLASSNAME(romClass);
		trace_printf(PORTLIB, tracefd, (char*)"Monitor=%p (%s monitor for %.*s @ %p) ", blockingMonitor, objType, J9UTF8_LENGTH(className), J9UTF8_DATA(className), object);
	} else {
		name = omrthread_monitor_get_name((omrthread_monitor_t)blockingMonitor);
		if (NULL == name) {
			name = (char*)"System monitor";
		}
		trace_printf(PORTLIB, tracefd, (char*)"Monitor=%p (%s) ", blockingMonitor,name);
	}

	trace_printf(PORTLIB, tracefd, (char*)"Count=%zu\n", count);

	if (owner) {
		ownerPtr = owner;
		ownerName = getOMRVMThreadName(owner->omrVMThread);
		trace_printf(PORTLIB, tracefd, (char*)"Owner=%s(%p)\n", ownerName, ownerPtr);
	} else if (blockingMonitor->owner) {
		ownerPtr = (J9VMThread*)blockingMonitor->owner;
		ownerName = (char*)"(unattached thread)";
		trace_printf(PORTLIB, tracefd, (char*)"Owner=%s(%p)\n", ownerName, ownerPtr);
	}

	if (owner) {
		releaseOMRVMThreadName(owner->omrVMThread);
	}
}

static void trace_printf(struct J9PortLibrary *portLib, IDATA tracefd, char * format, ...) {
	va_list args;
	char buffer[1024];
	I_32 wroteToFile = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	memset(buffer, 0, sizeof(buffer));
	va_start(args, format);
	j9str_vprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	if (tracefd != -1)
		wroteToFile = (j9file_write_text(tracefd, buffer, strlen(buffer)) != -1);
	if (!wroteToFile)
		j9tty_err_printf(PORTLIB, buffer);
}

j9object_t
createCachedOutOfMemoryError(J9VMThread * currentThread, j9object_t threadObject)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9MemoryManagerFunctions* gcFuncs = vm->memoryManagerFunctions;
	J9Class * jlOutOfMemoryError;
	j9object_t outOfMemoryError;

	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t)threadObject);
	jlOutOfMemoryError = internalFindKnownClass(currentThread, J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, J9_FINDKNOWNCLASS_FLAG_INITIALIZE);
	threadObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);


	outOfMemoryError = gcFuncs->J9AllocateObject(
		currentThread, jlOutOfMemoryError, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (outOfMemoryError != NULL) {
		J9Class * arrayClass;
		j9object_t walkback;

#ifdef J9VM_ENV_DATA64
		arrayClass = vm->longArrayClass;
#else
		arrayClass = vm->intArrayClass;
#endif
		PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, outOfMemoryError);
		walkback = gcFuncs->J9AllocateIndexableObject(
			currentThread, arrayClass, 32, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		outOfMemoryError = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
		if (walkback == NULL) {
			outOfMemoryError = NULL;
		} else {
			J9VMJAVALANGTHROWABLE_SET_WALKBACK(currentThread, outOfMemoryError, walkback);
		}
	}

	return outOfMemoryError;
}


UDATA
startJavaThread(J9VMThread * currentThread, j9object_t threadObject, UDATA privateFlags,
	UDATA osStackSize, UDATA priority, omrthread_entrypoint_t entryPoint, void * entryArg, j9object_t schedulingParameters)
{
	UDATA rc;
	UDATA setException = (privateFlags & J9_PRIVATE_FLAGS_NO_EXCEPTION_IN_START_JAVA_THREAD) == 0;
	j9object_t lock;
	j9object_t cachedOutOfMemoryError;
	J9JavaVM * vm = currentThread->javaVM;
	J9MemoryManagerFunctions* gcFuncs = vm->memoryManagerFunctions;

	/* Clear out bogus flag bit (used only to save having another parameter to this function) */

	privateFlags &= ~J9_PRIVATE_FLAGS_NO_EXCEPTION_IN_START_JAVA_THREAD;

#ifndef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
	/* Any attempt to start a Thread makes it illegal to attempt to start it again.
	 * Oracle class libraries don't have the 'started' field */
	J9VMJAVALANGTHREAD_SET_STARTED(currentThread, threadObject, TRUE);
#endif /* !J9VM_IVE_RAW_BUILD */

	/* Save objects on the stack in case we GC */

	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t) threadObject);
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, schedulingParameters);

	/* Create the cached out of memory error */

	cachedOutOfMemoryError = createCachedOutOfMemoryError(currentThread, threadObject);
	if (cachedOutOfMemoryError == NULL) {
		Trc_VM_startJavaThread_failedOOMAlloc(currentThread);
		rc = J9_THREAD_START_FAILED_OOM_ALLOCATION;
		goto pop2;
	}
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, cachedOutOfMemoryError);

	/* See if the thread object has been constructed - if not, allocate the lock object */

	threadObject = PEEK_OBJECT_IN_SPECIAL_FRAME(currentThread, 2);
	if (J9VMJAVALANGTHREAD_LOCK(currentThread, threadObject) == NULL) {
		J9Class * jlObject = J9VMJAVALANGOBJECT_OR_NULL(vm);

		lock = gcFuncs->J9AllocateObject(currentThread, jlObject, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (lock == NULL) {
			Trc_VM_startJavaThread_failedLockAlloc(currentThread);
			rc = J9_THREAD_START_FAILED_LOCK_OBJECT_ALLOCATION;
			goto pop3;
		}
	} else {
		lock = NULL;
	}
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, lock);

	rc = startJavaThreadInternal(currentThread, privateFlags, osStackSize, priority, entryPoint, entryArg, FALSE);
	if (rc != J9_THREAD_START_NO_ERROR) {
		Trc_VM_startJavaThread_gcAndRetry(currentThread);
		gcFuncs->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
		rc = startJavaThreadInternal(currentThread, privateFlags, osStackSize, priority, entryPoint, entryArg, setException);
	}

	/* Restore stack */

	DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
pop3:
	DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
pop2:
	DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	goto popNone; /* goto next line -- suppress warning about unused label */
popNone:
	return rc;
}

/* On entry the stack is:
 *
 * 0: lock
 * 1: OOM
 * 2: schedulingParameters
 * 3: threadObject
 */

static UDATA
startJavaThreadInternal(J9VMThread * currentThread, UDATA privateFlags, UDATA osStackSize, UDATA priority,
		omrthread_entrypoint_t entryPoint, void * entryArg, UDATA setException)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9VMThread * newThread;
	IDATA retVal;
 	j9object_t lock;
	omrthread_t osThread;
	j9object_t cachedOutOfMemoryError;
	j9object_t threadObject;
	char *threadName = NULL;

	/* Fork the OS thread */

	retVal = vm->internalVMFunctions->createThreadWithCategory(
				&osThread,
				osStackSize,
				vm->java2J9ThreadPriorityMap[priority],
				TRUE,
				entryPoint,
				entryArg,
				J9THREAD_CATEGORY_APPLICATION_THREAD);
	if (retVal != J9THREAD_SUCCESS) {
		if (retVal & J9THREAD_ERR_OS_ERRNO_SET) {
#if defined(J9ZOS390)
			omrthread_os_errno_t os_errno = omrthread_get_os_errno();
			omrthread_os_errno_t os_errno2 = omrthread_get_os_errno2();
			Trc_VM_startJavaThread_failedToCreateOSThreadWithErrno2(currentThread, -retVal,
				omrthread_get_errordesc(-retVal), os_errno, os_errno, os_errno2, os_errno2);
			if (setException) {
				if (0 == setFailedToForkThreadException(currentThread, -retVal, os_errno, os_errno2)) {
					return J9_THREAD_START_THROW_CURRENT_EXCEPTION;
				}
			}
#else /* !J9ZOS390 */
			omrthread_os_errno_t os_errno = omrthread_get_os_errno();
			Trc_VM_startJavaThread_failedToCreateOSThreadWithErrno(currentThread, -retVal,
				omrthread_get_errordesc(-retVal), os_errno, os_errno);
			if (setException) {
				if (0 == setFailedToForkThreadException(currentThread, -retVal, os_errno)) {
					return J9_THREAD_START_THROW_CURRENT_EXCEPTION;
				}
			}
#endif /* !J9ZOS390 */
		} else {
			Trc_VM_startJavaThread_failedToCreateOSThread(currentThread, -retVal,
				omrthread_get_errordesc(-retVal));
		}
		return J9_THREAD_START_FAILED_TO_FORK_THREAD;
	}

	/* Create the UTF8 string with the thread name */
	threadObject = PEEK_OBJECT_IN_SPECIAL_FRAME(currentThread, 3);
#ifdef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
	{
		j9object_t unicodeChars = J9VMJAVALANGTHREAD_NAME(currentThread, threadObject);
		if (NULL != unicodeChars) {
			threadName = copyStringToUTF8WithMemAlloc(currentThread, unicodeChars, J9_STR_NULL_TERMINATE_RESULT, "", 0, NULL, 0, NULL);
		}
	}
#else /* J9VM_IVE_RAW_BUILD */
	threadName = getVMThreadNameFromString(currentThread, J9VMJAVALANGTHREAD_NAME(currentThread, threadObject));
#endif /* J9VM_IVE_RAW_BUILD */
	if (threadName == NULL) {
		Trc_VM_startJavaThread_failedVMThreadAlloc(currentThread);
		omrthread_cancel(osThread);
		return J9_THREAD_START_FAILED_VMTHREAD_ALLOC;
	}

	/* Create the vmThread */

	newThread = allocateVMThread(vm, osThread, privateFlags, currentThread->omrVMThread->memorySpace, threadObject);
	if (newThread == NULL) {
		PORT_ACCESS_FROM_PORT(vm->portLibrary);

		Trc_VM_startJavaThread_failedVMThreadAlloc(currentThread);
		omrthread_cancel(osThread);
		j9mem_free_memory(threadName);
		return J9_THREAD_START_FAILED_VMTHREAD_ALLOC;
	}

	setOMRVMThreadNameWithFlag(currentThread->omrVMThread, newThread->omrVMThread, threadName, 0);
#if !defined(LINUX)
	/* on Linux this is done by the new thread when it starts running */
	omrthread_set_name(newThread->osThread, threadName);
#endif

	/* Link the thread and vmThread and fill in the thread fields */

	cachedOutOfMemoryError = PEEK_OBJECT_IN_SPECIAL_FRAME(currentThread, 1);
	lock = PEEK_OBJECT_IN_SPECIAL_FRAME(currentThread, 0);

	newThread->outOfMemoryError = cachedOutOfMemoryError;
	Assert_VM_true(newThread->threadObject == threadObject);
	if (lock != NULL) {
		J9VMJAVALANGTHREAD_SET_LOCK(currentThread, threadObject, lock);
	}
	J9VMJAVALANGTHREAD_SET_THREADREF(currentThread, threadObject, newThread);

#if (JAVA_SPEC_VERSION >= 14)
	/* If thread was interrupted before start, make sure interrupt flag is set for running thread. */
	if (J9VMJAVALANGTHREAD_DEADINTERRUPT(currentThread, threadObject)) {
		omrthread_interrupt(osThread);
	}
#endif

	/* Allow the thread to run */

	omrthread_resume(osThread);

	TRIGGER_J9HOOK_VM_THREAD_STARTING(vm->hookInterface, currentThread, newThread);

	return J9_THREAD_START_NO_ERROR;
}


#if defined(J9ZOS390)
/**
 * Set the current exception when omrthread_create() fails with an OS-specific errno.
 * @param currentThread Parent VM thread
 * @param retVal Internal return value from omrthread_create().
 * @param os_errno OS-specific error code
 * @param os_errno2 ZOS-specific error code. Value is 0 on non-ZOS platforms
 * @return success or failure
 * @retval 0 success
 * @retval -1 failure This function may fail if the NLS message can't be found, or if there is insufficient memory to construct the exception message.
 */
static IDATA
setFailedToForkThreadException(J9VMThread *currentThread, IDATA retVal, omrthread_os_errno_t os_errno, omrthread_os_errno_t os_errno2)
{
	IDATA rc = -1;
	const char *errorMessage;
	char *buf = NULL;
	UDATA bufLen = 0;
	PORT_ACCESS_FROM_VMC(currentThread);

	errorMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
			J9NLS_VM_THREAD_CREATE_FAILED_WITH_32BIT_ERRNO2, NULL);

	if (errorMessage) {
		bufLen = j9str_printf(PORTLIB, NULL, 0, errorMessage, retVal, os_errno, os_errno, (U_32)(UDATA)os_errno2);
		if (bufLen > 0) {
			buf = (char*)j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
			if (buf) {
				/* j9str_printf return value doesn't include the NUL terminator */
				if ((bufLen - 1) == j9str_printf(PORTLIB, buf, bufLen, errorMessage, retVal, os_errno, os_errno, os_errno2, os_errno2)) {
					setCurrentExceptionUTF(currentThread, J9_EX_OOM_THREAD | J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, buf);
					rc = 0;
				}
				j9mem_free_memory(buf);
			}
		}
	}

	return rc;
}
#else /* !J9ZOS390 */
/**
 * Set the current exception when omrthread_create() fails with an OS-specific errno.
 * @param currentThread Parent VM thread
 * @param retVal Internal return value from omrthread_create().
 * @param os_errno OS-specific error code
 * @return success or failure
 * @retval 0 success
 * @retval -1 failure This function may fail if the NLS message can't be found, or if there is insufficient memory to construct the exception message.
 */
static IDATA
setFailedToForkThreadException(J9VMThread *currentThread, IDATA retVal, omrthread_os_errno_t os_errno)
{
	IDATA rc = -1;
	const char *errorMessage;
	char *buf = NULL;
	UDATA bufLen = 0;
	PORT_ACCESS_FROM_VMC(currentThread);

	errorMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
		J9NLS_VM_THREAD_CREATE_FAILED_WITH_ERRNO, NULL);

	if (errorMessage) {
		bufLen = j9str_printf(PORTLIB, NULL, 0, errorMessage, retVal, os_errno);
		if (bufLen > 0) {
			buf = (char*)j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
			if (buf) {
				/* j9str_printf return value doesn't include the NUL terminator */
				if ((bufLen - 1) == j9str_printf(PORTLIB, buf, bufLen, errorMessage, retVal, os_errno)) {
					setCurrentExceptionUTF(currentThread, J9_EX_OOM_THREAD | J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, buf);
					rc = 0;
				}
				j9mem_free_memory(buf);
			}
		}
	}

	return rc;
}

#endif /* !J9ZOS390 */




static UDATA
javaProtectedThreadProc(J9PortLibrary* portLibrary, void * entryarg)
{
	J9VMThread * vmThread = (J9VMThread *) entryarg;
	J9JavaVM *vm = vmThread->javaVM;

	initializeCurrentOSStackFree(vmThread, vmThread->osThread, vm->defaultOSStackSize);

#if defined(LINUX)
	omrthread_set_name(vmThread->osThread, (char*)vmThread->omrVMThread->threadName);
#endif /* defined(LINUX) */

	threadAboutToStart(vmThread);

	TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, vmThread, vmThread);

	acquireVMAccess(vmThread);
#if defined(J9VM_OPT_DEPRECATED_METHODS) && (JAVA_SPEC_VERSION < 20)
	if (!(J9VMJAVALANGTHREAD_STOPCALLED(vmThread, vmThread->threadObject)))
#endif /* defined(J9VM_OPT_DEPRECATED_METHODS) && (JAVA_SPEC_VERSION < 20) */
	{
		/* Start running the thread. */
		runJavaThread(vmThread);
	}
	releaseVMAccess(vmThread);

	/* Perform thread cleanup */

	threadCleanup(vmThread, TRUE);

	/* Exit the OS thread */

	return 0;
}



/*
 * Deadlock detection
 */
typedef struct ThreadMap {
	J9VMThread *thread;
	IDATA index;
} ThreadMap;

static UDATA
tmapHash(void *key, void *userData)
{
	return (UDATA)((ThreadMap*)key)->thread;
}

static UDATA
tmapHashEqual(void *leftKey, void *rightKey, void *userData)
{
	return ((ThreadMap*)leftKey)->thread == ((ThreadMap*)rightKey)->thread;
}

/*
 * This function finds java threads that are deadlocked on object monitors.
 * The caller must have VM access.
 * The caller is responsible for freeing the memory allocated for
 * deadlockedThreads and blockingObjects (using portlib j9mem_free_memory()).
 * deadlockedThreads, blockingObjects are not changed if an error occurs.
 *
 * [in] currentThread		Must be non-NULL
 * [out]deadlockedThreads	Where to store array of returned vmThreads.
 * 								If NULL, this info is not returned.
 * [out]blockingObjects		Where to store the corresponding monitor that each deadlockedThread is blocked on.
 * 								If NULL, this info is not returned.
 * [in] flags				See J9VMTHREAD_FINDDEADLOCKFLAG_xxx in vm_api.h
 * Returns:
 * >= 0	number of deadlocked threads
 * <0	unexpected error (e.g. out of memory)
 */
IDATA
findObjectDeadlockedThreads(J9VMThread *currentThread,
	j9object_t **pDeadlockedThreads, j9object_t **pBlockingObjects,
	UDATA flags)
{
	typedef struct ThreadChain {
		J9VMThread *thread;
		j9object_t threadobj;
		j9object_t object;
		J9VMThread *owner;
		IDATA owner_index;
		UDATA visit;
		BOOLEAN deadlocked;
	} ThreadChain;

	PORT_ACCESS_FROM_VMC(currentThread);
	IDATA threadCount = 0;
	IDATA deadCount = -1;
	J9VMThread *vmThread;
	J9HashTable *hashTable;
	ThreadChain *thrChain;
	UDATA visit = 0;
	IDATA i, index;
	j9object_t *deadlockedThreads = NULL;
	j9object_t *blockingObjects = NULL;

	Assert_VM_mustHaveVMAccess(currentThread);

	if (!(flags & J9VMTHREAD_FINDDEADLOCKFLAG_ALREADYHAVEEXCLUSIVE)) {
		acquireExclusiveVMAccess(currentThread);
	}

	/* Count vmThreads: Loop over all vmThreads until we get back to the starting thread */
	threadCount = 0;
	vmThread = currentThread;
	do {
		if ((NULL != vmThread->threadObject)
#if JAVA_SPEC_VERSION >= 19
		/* Exclude J9VMThreads if a continuation is mounted to match the RI. */
		&& (NULL == vmThread->currentContinuation)
#endif /* JAVA_SPEC_VERSION >= 19 */
		) {
			++threadCount;
		}
	} while ((vmThread = vmThread->linkNext) != currentThread);

	if (threadCount <= 0) {
		deadCount = 0;
		goto findDeadlock_done;
	}

	/* Allocate the destination array */
	thrChain = (ThreadChain *)j9mem_allocate_memory(threadCount * sizeof(ThreadChain), OMRMEM_CATEGORY_VM);
	if (NULL == thrChain) {
		goto findDeadlock_fail;
	}

	/*
	 * Use a hash-table to map J9VMThreads to their index in the thrChain array
	 * (this allows us to walk the thrChain array in monitor-ownership chain order)
	 */
	hashTable = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 0, sizeof(ThreadMap), 0, 0, OMRMEM_CATEGORY_THREADS, tmapHash, tmapHashEqual, NULL, NULL);
	if (NULL == hashTable) {
		j9mem_free_memory(thrChain);
		goto findDeadlock_fail;
	}

	/*
	 * Loop over all vmThreads looking for blocked threads until we get back to the starting thread.
	 * Record blocked threads in the hash table.
	 */
	threadCount = 0;
	vmThread = currentThread;
	do {
		if ((NULL != vmThread->threadObject)
#if JAVA_SPEC_VERSION >= 19
		/* Exclude J9VMThreads if a continuation is mounted to match the RI. */
		&& (NULL == vmThread->currentContinuation)
#endif /* JAVA_SPEC_VERSION >= 19 */
		) {
			UDATA state;
			J9VMThread *owner;
			j9object_t monitorObject;
			BOOLEAN isBlocked = FALSE;

			state = getVMThreadObjectState(vmThread, &monitorObject, &owner, NULL);
			switch (state) {
			case J9VMTHREAD_STATE_BLOCKED:
				if (monitorObject && owner) {
					isBlocked = TRUE;
				}
				break;
			case J9VMTHREAD_STATE_WAITING:
				if (flags & J9VMTHREAD_FINDDEADLOCKFLAG_INCLUDEWAITING) {
					if (monitorObject && owner) {
						isBlocked = TRUE;
					}
				}
				break;
			case J9VMTHREAD_STATE_PARKED:
				if (flags & J9VMTHREAD_FINDDEADLOCKFLAG_INCLUDESYNCHRONIZERS) {
					if (monitorObject && owner) {
						isBlocked = TRUE;
					}
				}
				break;
			default:
				isBlocked = FALSE;
			}

			if (TRUE == isBlocked) {
				ThreadMap tmap;

				thrChain[threadCount].thread = vmThread;
				thrChain[threadCount].threadobj = (j9object_t)vmThread->threadObject;
				thrChain[threadCount].object = monitorObject;
				thrChain[threadCount].owner = owner;

				tmap.thread = vmThread;
				tmap.index = threadCount;
				hashTableAdd(hashTable, &tmap);

				++threadCount;
			}
		}
	} while ((vmThread = vmThread->linkNext) != currentThread);

	if (!(flags & J9VMTHREAD_FINDDEADLOCKFLAG_ALREADYHAVEEXCLUSIVE)) {
		releaseExclusiveVMAccess(currentThread);
	}

	/* Walk the thrChain array, filling in the owner_index fields, using the hash */
	for (i = 0; i < threadCount; ++i) {
		ThreadMap tmap;
		ThreadMap *p_tmap;

		/* Fake-up a ThreadMap for query purposes only */
		tmap.thread = thrChain[i].owner;
		p_tmap = (ThreadMap *)hashTableFind(hashTable, &(tmap));
		if (p_tmap == NULL) {
			/* Owner not mapped, so must be a non-blocked thread */
			thrChain[i].owner_index = -1;
		} else {
			thrChain[i].owner_index = p_tmap->index;
		}
		thrChain[i].visit = 0;
		thrChain[i].deadlocked = FALSE;
	}

	/* Done with the hash-table, free it */
	hashTableFree(hashTable);

	/* We're now ready to walk the thrChain array, looking for loops */
	deadCount = 0;
	for (i = 0; i < threadCount; ++i)  {
		++visit;
		index = i;
		do {
			if (thrChain[index].deadlocked || (thrChain[index].visit == visit)) {
				/* deadlock detected! Entire chain starting at thrChain[i] is deadlocked. */
				thrChain[i].deadlocked = TRUE;
				++deadCount;
				index = thrChain[i].owner_index;
				while (!thrChain[index].deadlocked) {
					thrChain[index].deadlocked = TRUE;
					++deadCount;
					index = thrChain[index].owner_index;
				}
				break;
			} else if (thrChain[index].visit > 0) {
				/* We've entered a non-deadlocked chain, so we're not deadlocked either */
				break;
			}
			thrChain[index].visit = visit;
			index = thrChain[index].owner_index;
		} while (index != -1);
	}

	if (deadCount <= 0) {
		j9mem_free_memory(thrChain);
		deadCount = 0;
		goto findDeadlock_done;
	}

	/* Finally, ready to output the list of deadlocked threads */
	if (pDeadlockedThreads) {
		deadlockedThreads = (j9object_t *)j9mem_allocate_memory(deadCount * sizeof(UDATA), OMRMEM_CATEGORY_VM);
		if (NULL == deadlockedThreads) {
			j9mem_free_memory(thrChain);
			return -1;
		}
	}

	if (pBlockingObjects) {
		blockingObjects = (j9object_t *)j9mem_allocate_memory(deadCount * sizeof(UDATA), OMRMEM_CATEGORY_VM);
		if (NULL == blockingObjects) {
			j9mem_free_memory(deadlockedThreads);
			j9mem_free_memory(thrChain);
			return -1;
		}
	}

	deadCount = 0;
	for (i = 0; i < threadCount; ++i) {
		if (thrChain[i].deadlocked) {
			if (deadlockedThreads) {
				deadlockedThreads[deadCount] = thrChain[i].threadobj;
			}
			if (blockingObjects) {
				blockingObjects[deadCount] = thrChain[i].object;
			}
			++deadCount;
		}
	}
	j9mem_free_memory(thrChain);

findDeadlock_done:
	if (deadCount >= 0) {
		if (pDeadlockedThreads) {
			*pDeadlockedThreads = deadlockedThreads;
		}
		if (pBlockingObjects) {
			*pBlockingObjects = blockingObjects;
		}
	}
	return deadCount;

findDeadlock_fail:
	if (!(flags & J9VMTHREAD_FINDDEADLOCKFLAG_ALREADYHAVEEXCLUSIVE)) {
		releaseExclusiveVMAccess(currentThread);
	}
	return -1;
}

/*
 * This function returns the Java priority for the thread specified.  For RealtimeThreads
 * the priority comes from the priority field in the PriorityParameters object for the thread, while
 * for regular Java threads or any thread in a non-realtime vm the priority comes from the priority field
 * in the Java Thread object
 *
 * Callers of this method must ensure that the Java Thread object for the thread is not NULL and
 * that this cannot change while the call is being made
 *
 * This method should also only be called when we don't need barrier checks such as call from
 * jvmti and ras
 *
 * @param vm the vm to be used to do the lookup
 * @param thread the thread for which to get the priority
 *
 * @retval the Java priority for the thread
 */
UDATA
getJavaThreadPriority(struct J9JavaVM *vm, J9VMThread* thread )
{
#if JAVA_SPEC_VERSION >= 19
	j9object_t threadHolder = J9VMJAVALANGTHREAD_HOLDER(thread, thread->threadObject);
	UDATA priority = 0;
	if (NULL != threadHolder) {
		priority = J9VMJAVALANGTHREADFIELDHOLDER_PRIORITY(thread, threadHolder);
	}

	return priority;
#else /* JAVA_SPEC_VERSION >= 19 */
	return J9VMJAVALANGTHREAD_PRIORITY(thread, thread->threadObject);
#endif /* JAVA_SPEC_VERSION >= 19 */
}


/**
 * Perform thread setup before any java code is run on the thread.
 * Triggers J9HOOK_THREAD_ABOUT_TO_START.
 *
 * @param currentThread the current J9VMThread
 */
void
threadAboutToStart(J9VMThread *currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9_SETUP_FPU_STATE();
#ifdef J9VM_JIT_FREE_SYSTEM_STACK_POINTER
	registerSystemStackPointerThreadOffset(currentThread);
#endif

#if defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390)
	/* Concurrent scavenge enabled and JIT loaded implies running on supported h/w.
	 * As such, per-thread initialization must occur
	 */
	if (vm->memoryManagerFunctions->j9gc_concurrent_scavenger_enabled(vm)
		&& (NULL != vm->jitConfig)
	) {
		if (0 == j9gs_initializeThread(currentThread)) {
			fatalError((JNIEnv *)currentThread, "Failed to initialize thread; please disable Concurrent Scavenge.\n");
		}
	}
#endif

	TRIGGER_J9HOOK_THREAD_ABOUT_TO_START(vm->hookInterface, currentThread);
}

} /* extern "C" */
