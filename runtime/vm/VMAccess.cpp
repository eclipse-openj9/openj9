/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

/* Note the required ordering of monitors related to VM access:
 *
 * vm->threadListMutex
 *	before
 * Any number of vmThread->publicFlagsMutex (there is one per thread)
 *	before
 * vm->exclusiveAccessMutex
 *
 * It is not necessary to always acquire all of the monitors, but whenever more than
 * one monitor is acquired, the above ordering must be maintained.
 */

/* #define J9VM_DBG */

#include <string.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "omrlinkedlist.h"
#include "omrthread.h"
#include "jni.h"
#include "vm_internal.h"
#include "ut_j9vm.h"
#include "j9javaPriority.h"
#include "vmaccess.h"
#include "omr.h"

#include "VMAccess.hpp"

extern "C" {

static void initializeExclusiveVMAccessStats(J9JavaVM* vm, J9VMThread* currentThread);
static U_64 updateExclusiveVMAccessStats(J9VMThread* currentThread);

#if (defined(J9VM_DBG))
static void badness (char *description);
#endif /* J9VM_DBG */

IDATA
internalTryAcquireVMAccess(J9VMThread *currentThread)
{
	return VM_VMAccess::inlineTryAcquireVMAccess(currentThread) ? 0 : -1;
}

void
internalAcquireVMAccess(J9VMThread *currentThread)
{
	VM_VMAccess::inlineAcquireVMAccessExhaustiveCheck(currentThread);
}

void
internalReleaseVMAccess(J9VMThread *currentThread)
{
	VM_VMAccess::inlineReleaseVMAccess(currentThread);
}

/**
 * Initialize the vm's J9ExclusiveVMStats structure at the beginning
 * of an exclusive access request.
 *
 * @parm[in] vm the VM to initialize
 * @parm[in] currentThread the thread requesting access, or NULL if external
 */
static void
initializeExclusiveVMAccessStats(J9JavaVM* vm, J9VMThread* currentThread)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	vm->omrVM->exclusiveVMAccessStats.startTime = j9time_hires_clock();
	vm->omrVM->exclusiveVMAccessStats.endTime = 0;
	vm->omrVM->exclusiveVMAccessStats.totalResponseTime = 0;
	vm->omrVM->exclusiveVMAccessStats.requester = (NULL == currentThread ? NULL : currentThread->omrVMThread);
	vm->omrVM->exclusiveVMAccessStats.lastResponder = (NULL == currentThread ? NULL : currentThread->omrVMThread);
	vm->omrVM->exclusiveVMAccessStats.haltedThreads = 0;
}

/**
 * Update the vm's J9ExclusiveVMStats structure once currentThread has responded.
 *
 * @parm[in] currentThread the thread responding
 *
 * @return the current time used for the statistics
 */
static U_64
updateExclusiveVMAccessStats(J9VMThread* currentThread)
{
	J9JavaVM* const vm = currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	return VM_VMAccess::updateExclusiveVMAccessStats(currentThread, vm, PORTLIB);
}


void  
acquireExclusiveVMAccess(J9VMThread * vmThread)
{
	UDATA responsesExpected = 0;
	UDATA jniCriticalResponsesExpected = 0;
	J9JavaVM* vm = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9VMThread * currentThread;

	Trc_VM_acquireExclusiveVMAccess_Entry(vmThread);
	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
	Assert_VM_true(0 == vmThread->safePointCount);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before acquiring exclusive
	Assert_VM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

	/* Check the exclusive count on this thread. If it's >1,
	 * the thread already has exclusive access
	 */
	if ( ++(vmThread->omrVMThread->exclusiveCount) == 1 ) {
		omrthread_monitor_enter(vmThread->publicFlagsMutex);
		VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		if ( J9_XACCESS_NONE != vm->exclusiveAccessState ) {
			UDATA reacquireJNICriticalAccess = FALSE;

			/* Exclusive VM Access is being claimed by another thread.
			 * Place this thread at the end of the queue and halt this one.
			 */
			Trc_VM_acquireExclusiveVMAccess_ExclusiveAccessHeldByAnotherThread(vmThread);

			if ( J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
				Trc_VM_acquireExclusiveVMAccess_QueueEmpty(vmThread);
				J9_LINEAR_LINKED_LIST_ADD(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,vmThread);
			} else {
				Trc_VM_acquireExclusiveVMAccess_QueueNonEmpty(vmThread,vm->exclusiveVMAccessQueueHead);
				J9_LINEAR_LINKED_LIST_ADD_AFTER(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,vm->exclusiveVMAccessQueueTail,vmThread);
			}
			/* Update the tail, because the macros don't do this. */
			vm->exclusiveVMAccessQueueTail = vmThread;

			VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_QUEUED_FOR_EXCLUSIVE);
			internalReleaseVMAccessNoMutex(vmThread);

			/* Set the flag _before_ releasing the exclusiveAccessMutex to prevent
			 * another thread from clearing the flag before we get to this line.
			 */
			if((vmThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE) == 0) {
				if(vmThread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
					/* If we haven't been counted yet, then we must not respond.
					 * Manually clear the JNI critical access flag before releasing
					 * and reacquiring VM access (and, thus, blocking).
					 */
					reacquireJNICriticalAccess = TRUE;
					VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS);
				}
				VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE);
			}
			omrthread_monitor_exit(vm->exclusiveAccessMutex);

			/* Wait on the public flags mutex, when the thread gets notified,
			 * it should continue to obtain exclusive VM access.
			 */
			Trc_VM_acquireExclusiveVMAccess_WaitingOnMutex(vmThread);
			internalAcquireVMAccessNoMutexWithMask(vmThread,J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE);
			VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_QUEUED_FOR_EXCLUSIVE);
			if(reacquireJNICriticalAccess) {
				/* Restore the manually cleared JNI critical access flag. */
				VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS);
			}
			omrthread_monitor_exit(vmThread->publicFlagsMutex);

			/* Initialize exclusive vm access here. */
			omrthread_monitor_enter(vm->exclusiveAccessMutex);
			Trc_VM_acquireExclusiveVMAccess_GrantingExclusiveAccess(vmThread);
			initializeExclusiveVMAccessStats(vm, vmThread);
			Assert_VM_true((J9_XACCESS_HANDING_OFF == vm->exclusiveAccessState) || (J9_XACCESS_HANDING_OFF_FROM_EXTERNAL_THREAD == vm->exclusiveAccessState));
			if (J9_XACCESS_HANDING_OFF == vm->exclusiveAccessState ) {
				Trc_VM_acquireExclusiveVMAccess_SettingResponsesExpected(vmThread);
				/* Wait for the thread that handed off to give up any access it holds.
				 * It will definitely be holding VM access, and may be holding JNI
				 * critical access as well. Assume it is, and releaseExclusiveVMAccess
				 * will adjust the count accordingly.
				 */
				responsesExpected = 1;
				jniCriticalResponsesExpected = 1;
			}
			vm->exclusiveAccessState = J9_XACCESS_HANDED_OFF;
			omrthread_monitor_exit(vm->exclusiveAccessMutex);

		} else {
			/* This is the first thread going for exclusive access */
			Trc_VM_acquireExclusiveVMAccess_GrantingExclusiveAccessFirst(vmThread);

			/* Grant exclusive access to the thread now */
			Assert_VM_true(J9_XACCESS_NONE == vm->exclusiveAccessState);
			vm->exclusiveAccessState = J9_XACCESS_PENDING;
			vm->exclusiveAccessResponseCount = 0;
			vm->jniCriticalResponseCount = 0;
			initializeExclusiveVMAccessStats(vm, vmThread);
			omrthread_monitor_exit(vm->exclusiveAccessMutex);
			omrthread_monitor_exit(vmThread->publicFlagsMutex);

			/* Post a halt request to all threads */
			omrthread_monitor_enter(vm->vmThreadListMutex);

#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
			currentThread = vmThread;
			while ((currentThread = currentThread->linkNext) != vmThread) {
				omrthread_monitor_enter(currentThread->publicFlagsMutex);
				VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE , true);
				/* Because the previous line writes atomically to the same field read below, there is likely
				 * no barrier required here, but do a full fence to be sure.  The barrier may also be required
				 * before reading inNative below.
				 */
				VM_AtomicSupport::readWriteBarrier();
				if(currentThread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
					/* Count threads in JNI critical regions who must respond.
					 * These will respond either by trying to acquire VM access
					 * or by exiting their outermost critical region.
					 */
					jniCriticalResponsesExpected++;
				}
				omrthread_monitor_exit(currentThread->publicFlagsMutex);
			}

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
			flushProcessWriteBuffers(vm);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

			Assert_VM_true(0 == vm->exclusiveAccessResponseCount);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */

			currentThread = vmThread;
			while ((currentThread = currentThread->linkNext) != vmThread) {
				omrthread_monitor_enter(currentThread->publicFlagsMutex);
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE, true);
				/* Because the previous line writes atomically to the same field read below, there is likely
				 * no barrier required here, but do a full fence to be sure.  The barrier may also be required
				 * before reading inNative below.
				 */
				VM_AtomicSupport::readWriteBarrier();
				if(currentThread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
					/* Count threads in JNI critical regions who must respond.
					 * These will respond either by trying to acquire VM access
					 * or by exiting their outermost critical region.
					 */
					jniCriticalResponsesExpected++;
				}
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
				if (currentThread->inNative) {
					TRIGGER_J9HOOK_VM_ACQUIRING_EXCLUSIVE_IN_NATIVE(vm->hookInterface, vmThread, currentThread);
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
					VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
				} else if (currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
					VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
					responsesExpected++;
				}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
				if (currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
					VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
					responsesExpected++;
				}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
				omrthread_monitor_exit(currentThread->publicFlagsMutex);
			}
			omrthread_monitor_exit(vm->vmThreadListMutex);
			Trc_VM_acquireExclusiveVMAccess_PostedHaltRequest(vmThread,responsesExpected);
		}

		/* Wait for all threads to respond to the halt request  */
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		vm->exclusiveAccessResponseCount += responsesExpected;
		Trc_VM_acquireExclusiveVMAccess_WaitingForResponses(vmThread,vm->exclusiveAccessResponseCount);
		while(vm->exclusiveAccessResponseCount) {
			omrthread_monitor_wait(vm->exclusiveAccessMutex);
		}

		/*
		 * Another thread may enter a critical region after the above HALT loop without
		 * being counted in jniCriticalResponsesExpected. Such threads increment vm->jniCriticalResponseCount.
		 */
		vm->jniCriticalResponseCount += jniCriticalResponsesExpected;
		Trc_VM_acquireExclusiveVMAccess_WaitingForJNICriticalRegionResponses(vmThread,vm->jniCriticalResponseCount);
		while(0 != vm->jniCriticalResponseCount) {
			/*
			 * This wait could be given a timeout to allow long (or blocked)
			 * critical regions to be interrupted by exclusive requests.
			 */
			omrthread_monitor_wait(vm->exclusiveAccessMutex);
		}

		Trc_VM_acquireExclusiveVMAccess_ChangingStateExclusive(vmThread);
		Assert_VM_true((J9_XACCESS_PENDING == vm->exclusiveAccessState) || (J9_XACCESS_HANDED_OFF == vm->exclusiveAccessState));
		vm->exclusiveAccessState = J9_XACCESS_EXCLUSIVE;
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
		omrthread_monitor_enter(vm->vmThreadListMutex);

		vm->omrVM->exclusiveVMAccessStats.endTime = j9time_hires_clock();
	}
	Assert_VM_true(J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState);
	Trc_VM_acquireExclusiveVMAccess_Exit(vmThread);
}



void   internalAcquireVMAccessClearStatus(J9VMThread * vmThread, UDATA flags)
{
	VM_VMAccess::inlineAcquireVMAccessClearStatus(vmThread, flags);
}


void   internalAcquireVMAccessNoMutexWithMask(J9VMThread * vmThread, UDATA haltMask)
{
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before acquiring access
	Assert_VM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	UDATA reacquireJNICriticalAccess = FALSE;
	J9JavaVM* vm = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustNotHaveVMAccess(vmThread);

	if(J9_ARE_ALL_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS)) {
		/* In a critical region and about to block acquiring VM access. */
		reacquireJNICriticalAccess = TRUE;
		VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS);

		if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE)) {
			omrthread_monitor_enter(vm->exclusiveAccessMutex);

			U_64 timeNow = updateExclusiveVMAccessStats(vmThread);

			--vm->jniCriticalResponseCount;
			if(vm->jniCriticalResponseCount == 0) {
				U_64 timeTaken = j9time_hires_delta(vm->omrVM->exclusiveVMAccessStats.startTime, timeNow, J9PORT_TIME_DELTA_IN_MILLISECONDS);

				UDATA slowTolerance = J9_EXCLUSIVE_SLOW_TOLERANCE_STANDARD;
				if (OMR_GC_ALLOCATION_TYPE_SEGREGATED == vm->gcAllocationType) {
					slowTolerance = J9_EXCLUSIVE_SLOW_TOLERANCE_REALTIME;
				}
				if (timeTaken > slowTolerance) {
					TRIGGER_J9HOOK_VM_SLOW_EXCLUSIVE(vm->hookInterface, vmThread, (UDATA) timeTaken, J9_EXCLUSIVE_SLOW_REASON_JNICRITICAL);
				}
				omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
			}
			omrthread_monitor_exit(vm->exclusiveAccessMutex);
		}
	}


	while (vmThread->publicFlags & haltMask) {
		omrthread_monitor_wait(vmThread->publicFlagsMutex);
	}

	TRIGGER_J9HOOK_VM_ACQUIREVMACCESS(vm->hookInterface, vmThread);

	/* Now that the hook has been invoked, allow inline VM access */
	if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS)) {
		VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS);
	}

	if(reacquireJNICriticalAccess) {
		VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS);
	} else {
		VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
}


void   internalAcquireVMAccessNoMutex(J9VMThread * vmThread)
{
	internalAcquireVMAccessNoMutexWithMask(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_ANY);
}


void   internalAcquireVMAccessWithMask(J9VMThread * vmThread, UDATA haltMask)
{
    omrthread_monitor_enter(vmThread->publicFlagsMutex);

    internalAcquireVMAccessNoMutexWithMask(vmThread, haltMask);

    omrthread_monitor_exit(vmThread->publicFlagsMutex);
}


static void  internalReleaseVMAccessNoMutexNoCheck(J9VMThread * vmThread)
{
	J9JavaVM* vm = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustHaveVMAccess(vmThread);

	VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_VM_ACCESS, true);

	TRIGGER_J9HOOK_VM_RELEASEVMACCESS(vm->hookInterface, vmThread);

	/* Now that the hook has been invoked, allow inline VM access */
	if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS)) {
		VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS);
	}

	if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_EXCLUSIVE_RESPONSE_MASK)) {
		J9JavaVM * vm = vmThread->javaVM;
		Trc_VM_internalReleaseVMAccessNoMutex_ThreadIsHalted(vmThread);

		omrthread_monitor_enter(vm->exclusiveAccessMutex);

		if (J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE == (vmThread->publicFlags & (J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE))) {
			U_64 timeNow = updateExclusiveVMAccessStats(vmThread);

			--vm->exclusiveAccessResponseCount;
			if (vm->exclusiveAccessResponseCount == 0) {
				VM_VMAccess::respondToExclusiveRequest(vmThread, vm, PORTLIB, timeNow, J9_EXCLUSIVE_SLOW_REASON_EXCLUSIVE);
			}
		}
		if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT)) {
			if (J9_ARE_NO_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT)) {
				VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT);
				VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT);
				if (J9_ARE_NO_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT)) {
					--vm->safePointResponseCount;
					if (vm->safePointResponseCount == 0) {
						omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
					}
				}
			}
		}

		omrthread_monitor_exit(vm->exclusiveAccessMutex);
	}

	Assert_VM_mustNotHaveVMAccess(vmThread);
	Trc_VM_internalReleaseVMAccessNoMutex_Exit(vmThread);
}


void  internalReleaseVMAccessNoMutex(J9VMThread * vmThread)
{
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before releasing access
	Assert_VM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	internalReleaseVMAccessNoMutexNoCheck(vmThread);
}


void  internalReleaseVMAccessSetStatus(J9VMThread * vmThread, UDATA flags)
{
	VM_VMAccess::inlineReleaseVMAccessSetStatus(vmThread, flags);
}


void releaseExclusiveVMAccess(J9VMThread * vmThread)
{
	J9JavaVM* vm = vmThread->javaVM;

	Trc_VM_releaseExclusiveVMAccess_Entry(vmThread);
	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
	Assert_VM_false(vmThread->omrVMThread->exclusiveCount == 0);
	Assert_VM_true(J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState);

	if (--(vmThread->omrVMThread->exclusiveCount) == 0) {
		/* Acquire these monitors in the same order as in allocateVMThread to prevent deadlock */

		/* Check the exclusive access queue */
		omrthread_monitor_enter(vmThread->publicFlagsMutex);
		VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);
		omrthread_monitor_enter(vm->exclusiveAccessMutex);

		if ( !J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
			J9VMThread * nextThread;

			vm->exclusiveAccessState = J9_XACCESS_HANDING_OFF;

			Trc_VM_releaseExclusiveVMAccess_QueueNonEmpty(vmThread,vm->exclusiveVMAccessQueueHead);
			/* The next thread will be waiting for a response from this one, so this thread
			 * had better not have J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE set.
			 */
			Assert_VM_false(J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE));
			/* Set the halt flag on the current thread */
			VM_VMAccess::setPublicFlags(vmThread,J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE, true);
			/* The thread accepting the hand off will be expecting
			 * responses for both VM and JNI critical access.
			 */
			if((vmThread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) == J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
				/* This thread is holding JNI critical access so it must run
				 * until it gives it up before the hand off can complete.
				 */
				vm->jniCriticalResponseCount = 0;
			} else {
				/* This thread does not hold critical access. Preemptively
				 * decrement the response count so the accepting thread
				 * will not wait.
				 */
				vm->jniCriticalResponseCount = -1;
			}
			omrthread_monitor_exit(vmThread->publicFlagsMutex);

			/* Queue is nonempty so clear the halt flags on the next thread only */
			nextThread = vm->exclusiveVMAccessQueueHead;
			Trc_VM_releaseExclusiveVMAccess_QueueNonEmpty2(vmThread,vm->exclusiveVMAccessQueueHead);

			J9_LINEAR_LINKED_LIST_REMOVE(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,nextThread);
			/* If the queue is now empty, set the tail to NULL as well */
			if ( J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
				vm->exclusiveVMAccessQueueTail = NULL;
			}

			/* Make sure the thread we've dequeued isn't pointing to another thread in the queue
			 * now that it's no longer part of the linked list.
			 */
			nextThread->exclusiveVMAccessQueueNext = NULL;
			VM_VMAccess::clearPublicFlags(nextThread,J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
			omrthread_monitor_exit(vm->exclusiveAccessMutex);

			omrthread_monitor_enter(nextThread->publicFlagsMutex);
			omrthread_monitor_notify_all(nextThread->publicFlagsMutex);
			omrthread_monitor_exit(nextThread->publicFlagsMutex);
		} else {
			J9VMThread *currentThread = vmThread;
			/* Queue is empty, so wake up all previously halted threads and notify the exclusiveAccessMutex */

			/* If using the fast class hash table, now is a good time to free the old tables */
			if (J9_ARE_ALL_BITS_SET(vmThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FAST_CLASS_HASH_TABLE)) {
				if ((NULL != vm->classLoaderBlocks) && (TRUE == vm->freePreviousClassLoaders)) {
					pool_state clState;
					J9ClassLoader *loader;

					omrthread_monitor_enter(vm->classLoaderBlocksMutex);
					loader = (J9ClassLoader*)pool_startDo(vm->classLoaderBlocks, &clState);
					while (loader != NULL) {
						J9HashTable *initial = loader->classHashTable;
						/* The GC may free the hash table but not the loader */
						if (NULL != initial) {
							J9HashTable *previous = initial->previous;
							while (NULL != previous) {
								Trc_VM_VMAccess_FreeingPreviousHashtable(currentThread, previous);
								J9HashTable *temp = previous->previous;
								hashTableFree(previous);
								previous = temp;
							}
							initial->previous = NULL;
						}
						loader = (J9ClassLoader*)pool_nextDo(&clState);
					}
					vm->freePreviousClassLoaders = FALSE;
					omrthread_monitor_exit(vm->classLoaderBlocksMutex);
				}
			}

			/* omrthread_monitor_enter(vm->vmThreadListMutex); current thread already holds vmThreadListMutex */
			/* Remove the Exclusive Access bit from the vm's exAccess state only if the queue is empty */
			Trc_VM_releaseExclusiveVMAccess_QueueEmpty(vmThread);
			vm->exclusiveAccessState = J9_XACCESS_NONE;
			/* Make sure stale thread pointers don't exist in the stats */
			vm->omrVM->exclusiveVMAccessStats.requester = NULL;
			vm->omrVM->exclusiveVMAccessStats.lastResponder = NULL;
			/* Free any cached decompilation records */
			PORT_ACCESS_FROM_JAVAVM(vm);
			j9mem_free_memory(currentThread->lastDecompilation);
			currentThread->lastDecompilation = NULL;
			while ((currentThread = currentThread->linkNext) != vmThread) {
				j9mem_free_memory(currentThread->lastDecompilation);
				currentThread->lastDecompilation = NULL;
				VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
			}
			omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
			omrthread_monitor_exit(vm->exclusiveAccessMutex);
			omrthread_monitor_exit(vmThread->publicFlagsMutex);

			while ((currentThread = currentThread->linkNext) != vmThread) {
				omrthread_monitor_enter(currentThread->publicFlagsMutex);
				omrthread_monitor_notify_all(currentThread->publicFlagsMutex);
				omrthread_monitor_exit(currentThread->publicFlagsMutex);
			}
		}

		omrthread_monitor_exit(vm->vmThreadListMutex);
	}

	Assert_VM_mustHaveVMAccess(vmThread);
	Trc_VM_releaseExclusiveVMAccess_Exit(vmThread);
}


IDATA   internalTryAcquireVMAccessNoMutexWithMask(J9VMThread * vmThread, UDATA haltMask)
{
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before acquiring access
	Assert_VM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	J9JavaVM* vm = vmThread->javaVM;

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustNotHaveVMAccess(vmThread);

    if (vmThread->publicFlags & haltMask) {
        return -1;
    }

	TRIGGER_J9HOOK_VM_ACQUIREVMACCESS(vm->hookInterface, vmThread);

	/* Now that the hook has been invoked, allow inline VM access */
	if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS)) {
		VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS);
	}

    VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS);

    return 0;
}

IDATA   internalTryAcquireVMAccessWithMask(J9VMThread * vmThread, UDATA haltMask)
{
    IDATA ret;

    omrthread_monitor_enter(vmThread->publicFlagsMutex);

    ret = internalTryAcquireVMAccessNoMutexWithMask(vmThread, haltMask);

    omrthread_monitor_exit(vmThread->publicFlagsMutex);

    return ret;
}


IDATA   internalTryAcquireVMAccessNoMutex(J9VMThread * vmThread)
{
    return internalTryAcquireVMAccessNoMutexWithMask(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_ANY);
}



/* --- vm access requests from external threads ---
* Returns FALSE if another party requested it already and we did not want to block.
* Otherwise returns TRUE.
*/
static BOOLEAN
synchronizeRequestsFromExternalThread(J9JavaVM * vm, BOOLEAN block)
{
	omrthread_monitor_enter(vm->exclusiveAccessMutex);
	while (J9_XACCESS_NONE != vm->exclusiveAccessState) {
		if (block) {
			omrthread_monitor_wait(vm->exclusiveAccessMutex);
		} else {
			omrthread_monitor_exit(vm->exclusiveAccessMutex);
			return FALSE;
		}
	}
	vm->exclusiveAccessState = J9_XACCESS_EXCLUSIVE;
	vm->exclusiveAccessResponseCount = 0;
	vm->jniCriticalResponseCount = 0;
	initializeExclusiveVMAccessStats(vm, NULL);
	omrthread_monitor_exit(vm->exclusiveAccessMutex);

	return TRUE;
}

static void
waitForResponseFromExternalThread(J9JavaVM * vm, UDATA vmResponsesExpected, UDATA jniResponsesExpected)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	omrthread_monitor_enter(vm->exclusiveAccessMutex);
	vm->exclusiveAccessResponseCount += vmResponsesExpected;
	while (vm->exclusiveAccessResponseCount) {
		omrthread_monitor_wait(vm->exclusiveAccessMutex);
	}

#if !defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	if(jniResponsesExpected > 0) {
		vm->jniCriticalResponseCount += jniResponsesExpected;
		while(vm->jniCriticalResponseCount != 0) {
			/* This wait could be given a timeout to allow long (or blocked)
			 * critical regions to be interrupted by exclusive requests.
			 */
			omrthread_monitor_wait(vm->exclusiveAccessMutex);
		}
	}
#endif /* !J9VM_INTERP_ATOMIC_FREE_JNI */

	omrthread_monitor_exit(vm->exclusiveAccessMutex);
	omrthread_monitor_enter(vm->vmThreadListMutex);

	vm->omrVM->exclusiveVMAccessStats.endTime = j9time_hires_clock();
}

void
acquireExclusiveVMAccessFromExternalThread(J9JavaVM * vm)
{
	J9VMThread * currentThread;
	UDATA vmResponsesExpected = 0;
	UDATA jniResponsesExpected = 0;

	synchronizeRequestsFromExternalThread(vm, TRUE);

	/* Post the halt request to all threads */

	omrthread_monitor_enter(vm->vmThreadListMutex);
	currentThread = vm->mainThread;

#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
	do {
		omrthread_monitor_enter(currentThread->publicFlagsMutex);
		VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE , true);
		/* Because the previous line writes atomically to the same field read below, there is likely
		 * no barrier required here, but do a full fence to be sure.  The barrier may also be required
		 * before reading inNative below.
		 */
		VM_AtomicSupport::readWriteBarrier();
		if(currentThread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
			/* Count threads in JNI critical regions who must respond.
			 * These will respond either by trying to acquire VM access
			 * or by exiting their outermost critical region.
			 */
			++jniResponsesExpected;
		}
		omrthread_monitor_exit(currentThread->publicFlagsMutex);
	} while ((currentThread = currentThread->linkNext) != vm->mainThread);

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
	flushProcessWriteBuffers(vm);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */

	do {
		omrthread_monitor_enter(currentThread->publicFlagsMutex);
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
		VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE, true);
		/* Because the previous line writes atomically to the same field read below, there is likely
		 * no barrier required here, but do a full fence to be sure.  The barrier may also be required
		 * before reading inNative below.
		 */
		VM_AtomicSupport::readWriteBarrier();
		if(currentThread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
			/* Count threads in JNI critical regions who must respond.
			 * These will respond either by trying to acquire VM access
			 * or by exiting their outermost critical region.
			 */
			++jniResponsesExpected;
		}
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		if (currentThread->inNative) {
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
			VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
		} else if (currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
			VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
			vmResponsesExpected++;
		}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
		if (currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
			VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
			vmResponsesExpected++;
		}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		omrthread_monitor_exit(currentThread->publicFlagsMutex);
	} while ((currentThread = currentThread->linkNext) != vm->mainThread);
	omrthread_monitor_exit(vm->vmThreadListMutex);

	/* Wait for all threads to respond to the halt request */

	waitForResponseFromExternalThread(vm, vmResponsesExpected, jniResponsesExpected);

}

void
releaseExclusiveVMAccessFromExternalThread(J9JavaVM * vm)
{
	J9VMThread * currentThread;
	Assert_VM_true(J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState);

	/* Acquire these monitors in the same order as in allocateVMThread to prevent deadlock */

	/* omrthread_monitor_enter(vm->vmThreadListMutex); current thread already holds vmThreadListMutex */
	omrthread_monitor_enter(vm->exclusiveAccessMutex);

	/* Before waking up halted threads, check the exclusive access queue */
	if ( !J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
		J9VMThread * nextThread;

		vm->exclusiveAccessState = J9_XACCESS_HANDING_OFF_FROM_EXTERNAL_THREAD;

		nextThread = vm->exclusiveVMAccessQueueHead;

		J9_LINEAR_LINKED_LIST_REMOVE(exclusiveVMAccessQueueNext,exclusiveVMAccessQueuePrevious,vm->exclusiveVMAccessQueueHead,nextThread);
		/* If the queue is now empty, set the tail to NULL as well */
		if ( J9_LINEAR_LINKED_LIST_IS_EMPTY(vm->exclusiveVMAccessQueueHead) ) {
			vm->exclusiveVMAccessQueueTail = NULL;
		}
		nextThread->exclusiveVMAccessQueueNext = NULL;
		VM_VMAccess::clearPublicFlags(nextThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
		omrthread_monitor_enter(nextThread->publicFlagsMutex);
		omrthread_monitor_notify_all(nextThread->publicFlagsMutex);
		omrthread_monitor_exit(nextThread->publicFlagsMutex);

	} else {
		vm->exclusiveAccessState = J9_XACCESS_NONE;
		currentThread = vm->mainThread;
		do {
			VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
		} while ((currentThread = currentThread->linkNext) != vm->mainThread);
		omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
		do {
				omrthread_monitor_enter(currentThread->publicFlagsMutex);
				omrthread_monitor_notify_all(currentThread->publicFlagsMutex);
				omrthread_monitor_exit(currentThread->publicFlagsMutex);
		} while ((currentThread = currentThread->linkNext) != vm->mainThread);
	}
	omrthread_monitor_exit(vm->vmThreadListMutex);
}

#if (defined(J9VM_GC_REALTIME))

UDATA
requestExclusiveVMAccessMetronome(J9JavaVM *vm, UDATA block, UDATA *responsesRequired, UDATA *gcPriority)
{
	/* TODO pass a dummy variable for jniResponsesRequired for now */
	UDATA jniResponsesRequired = 0;
	return requestExclusiveVMAccessMetronomeTemp(vm, block, responsesRequired, &jniResponsesRequired, gcPriority);
}

/**
 Metronome exclusive access acquisition proceeds in two phases.  The request comes from the metronome alarm thread and
 returns immediately with the priority at which the gc collector threads should run.  The main collector thread running at the
 required priority completes the acquisition of vm access for gc by waiting for the responses from the mutator threads,
*/
UDATA
requestExclusiveVMAccessMetronomeTemp(J9JavaVM *vm, UDATA block, UDATA *vmResponsesRequired, UDATA *jniResponsesRequired, UDATA *gcPriority)
{
	J9VMThread *thread;
	J9VMThread *mainThread;
	UDATA vmResponsesExpected = 0;
	UDATA jniResponsesExpected = 0;
	*gcPriority = J9THREAD_PRIORITY_MAX;

	/* Check if another party is requesting X access already. */
	if (FALSE == synchronizeRequestsFromExternalThread(vm, block)) {
		/* Yes, there was another party requesting X access, but because we did not want to block,
		 * we just give up, and let the caller know the request did not succeed */
		return FALSE;
	}

	/* Post the halt request to all threads */

	omrthread_monitor_enter(vm->vmThreadListMutex);
	mainThread = thread = vm->mainThread;

#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
	do {
		if (!(thread->privateFlags & J9_PRIVATE_FLAGS_GC_MASTER_THREAD)) {
			omrthread_monitor_enter(thread->publicFlagsMutex);
			VM_VMAccess::setPublicFlags(thread,J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE, true);
			/* Because the previous line writes atomically to the same field read below, there is likely
			 * no barrier required here, but do a full fence to be sure.  The barrier may also be required
			 * before reading inNative below.
			 */
			VM_AtomicSupport::readWriteBarrier();
			if(thread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
				/* Count threads in JNI critical regions who must respond.
				 * These will respond either by trying to acquire VM access
				 * or by exiting their outermost critical region.
				 */
				++jniResponsesExpected;
			} /* thread has jni critical access */
			omrthread_monitor_exit(thread->publicFlagsMutex);
		}
		thread = thread->linkNext;
	} while (thread != mainThread);

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
	flushProcessWriteBuffers(vm);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */

	do {
		if (!(thread->privateFlags & J9_PRIVATE_FLAGS_GC_MASTER_THREAD)) {
			omrthread_monitor_enter(thread->publicFlagsMutex);
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
			VM_VMAccess::setPublicFlags(thread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE, true);
			/* Because the previous line writes atomically to the same field read below, there is likely
			 * no barrier required here, but do a full fence to be sure.  The barrier may also be required
			 * before reading inNative below.
			 */
			VM_AtomicSupport::readWriteBarrier();
			if(thread->publicFlags & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS) {
				/* Count threads in JNI critical regions who must respond.
				 * These will respond either by trying to acquire VM access
				 * or by exiting their outermost critical region.
				 */
				++jniResponsesExpected;
			} /* thread has jni critical access */
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			if (thread->inNative) {
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::setPublicFlags(thread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
			} else if (thread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::clearPublicFlags(thread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVESH */
				++vmResponsesExpected;
		 	} /* thread has vm access */
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
 			if (thread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::clearPublicFlags(thread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVESH */
				++vmResponsesExpected;
 			} /* thread has vm access */
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

			omrthread_monitor_exit(thread->publicFlagsMutex);
		}
		thread = thread->linkNext;
	} while (thread != mainThread);
	omrthread_monitor_exit(vm->vmThreadListMutex);
	*vmResponsesRequired = vmResponsesExpected;
	*jniResponsesRequired = jniResponsesExpected;
	return TRUE;
}

void
waitForExclusiveVMAccessMetronome(J9VMThread * vmThread, UDATA responsesRequired)
{
	/* TODO kmt: pass 0 for jniResponsesRequired for now */
	waitForExclusiveVMAccessMetronomeTemp(vmThread, responsesRequired, 0);
}

void
waitForExclusiveVMAccessMetronomeTemp(J9VMThread * vmThread, UDATA vmResponsesRequired, UDATA jniResponsesRequired)
{
	waitForResponseFromExternalThread(vmThread->javaVM, vmResponsesRequired, jniResponsesRequired);

	VM_VMAccess::backOffFromSafePoint(vmThread);

	/* Do not wait on JAVA_SUSPEND, since we are not real JAVA thread.
	 * This patch was needed for some debug scenarios with JVMTI, that would result in a deadlock otherwise.
	 * TODO: re-examine with latest JDWP
	 */
	internalAcquireVMAccessNoMutexWithMask(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_ANY_NO_JAVA_SUSPEND);

	Assert_VM_true(vmThread->omrVMThread->exclusiveCount==0);
	
	++(vmThread->omrVMThread->exclusiveCount);
}

void
releaseExclusiveVMAccessMetronome(J9VMThread * vmThread)
{
	--(vmThread->omrVMThread->exclusiveCount);
	
	Assert_VM_true(vmThread->omrVMThread->exclusiveCount==0);
	
	internalReleaseVMAccessNoMutex(vmThread);

	releaseExclusiveVMAccessFromExternalThread(vmThread->javaVM);
}

#endif /* defined(J9VM_GC_REALTIME) */

void
mustHaveVMAccess(J9VMThread * vmThread)
{
	J9JavaVM* vm = vmThread->javaVM;
	UDATA CurrentThreadDoesNotHaveVMAccess = FALSE;

	/* This is already a failure case, so no need to guard with the debug mode check */

	Assert_VM_true(currentVMThread(vm) == vmThread);

	/* VM access was already checked in the caller, so don't check again - just fail out immediately */

	Assert_VM_true(CurrentThreadDoesNotHaveVMAccess);
}

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)

void
internalEnterVMFromJNI(J9VMThread *currentThread)
{
	currentThread->inNative = FALSE;
	VM_AtomicSupport::readWriteBarrier(); // necessary?
	if (J9_UNEXPECTED(currentThread->publicFlags != J9_PUBLIC_FLAGS_VM_ACCESS)) {
		omrthread_monitor_t const publicFlagsMutex = currentThread->publicFlagsMutex;
		omrthread_t const osThread = currentThread->osThread;
		omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
		/* If VM access acquire is being forced out-of-line, do the release to force a new
		 * acquire (e.g. for concurrent scavenger).
		 */
		if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_RELEASE_ACCESS_REQUIRED_MASK | J9_PUBLIC_FLAGS_VMACCESS_OUTOFLINE_MASK)) {
			if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS)) {
				internalReleaseVMAccessNoMutex(currentThread);
			}
		}
		if (!J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS)) {
			internalAcquireVMAccessNoMutex(currentThread);
		}
		omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
	}
}

void
internalExitVMToJNI(J9VMThread *currentThread)
{
	VM_AtomicSupport::writeBarrier();
	currentThread->inNative = TRUE;
	VM_AtomicSupport::readWriteBarrier(); // necessary?
	if (J9_UNEXPECTED(currentThread->publicFlags != J9_PUBLIC_FLAGS_VM_ACCESS)) {
		omrthread_monitor_t const publicFlagsMutex = currentThread->publicFlagsMutex;
		omrthread_t const osThread = currentThread->osThread;
		omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
		if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_RELEASE_ACCESS_REQUIRED_MASK | J9_PUBLIC_FLAGS_VMACCESS_OUTOFLINE_MASK)) {
			if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS)) {
				internalReleaseVMAccessNoMutexNoCheck(currentThread);
			}
		}
		omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
	}
}

#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

void  
acquireSafePointVMAccess(J9VMThread * vmThread)
{
	J9JavaVM* vm = vmThread->javaVM;
	UDATA responsesExpected = 0;
	J9VMThread * currentThread;

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
	Assert_VM_false(J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT));
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before acquiring exclusive
	Assert_VM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

	/* Check the exclusive count on this thread. If it's >1,
	 * the thread already has exclusive access
	 */
	if ( ++(vmThread->safePointCount) == 1 ) {
		Assert_VM_true(0 == vmThread->omrVMThread->exclusiveCount);
		internalReleaseVMAccess(vmThread);
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		while(J9_XACCESS_NONE != vm->safePointState) {
			omrthread_monitor_wait(vm->exclusiveAccessMutex);
		}
		/* Grant safe point access to the thread now */
		vm->safePointState = J9_XACCESS_PENDING;
		vm->safePointResponseCount = 0;
		omrthread_monitor_exit(vm->exclusiveAccessMutex);

		/* Post a safe point request to all threads */
		omrthread_monitor_enter(vm->vmThreadListMutex);

#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
		currentThread = vmThread;
		do {
			omrthread_monitor_enter(currentThread->publicFlagsMutex);
			VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT, true);
			/* Because the previous line writes atomically, there is likely no barrier required here, but do
			 * a full fence to be sure.  The barrier may also be required before reading inNative below.
			 */
			VM_AtomicSupport::readWriteBarrier();
			omrthread_monitor_exit(currentThread->publicFlagsMutex);
		} while ((currentThread = currentThread->linkNext) != vmThread);

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
		flushProcessWriteBuffers(vm);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */

		Assert_VM_true(0 == vm->safePointResponseCount);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */

		currentThread = vmThread;
		do {
			omrthread_monitor_enter(currentThread->publicFlagsMutex);
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
			VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT, true);
			/* Because the previous line writes atomically, there is likely no barrier required here, but do
			 * a full fence to be sure.  The barrier may also be required before reading inNative below.
			 */
			VM_AtomicSupport::readWriteBarrier();
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			if (currentThread->inNative) {
				TRIGGER_J9HOOK_VM_ACQUIRING_EXCLUSIVE_IN_NATIVE(vm->hookInterface, vmThread, currentThread);
				Assert_VM_false(J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT));
#if !defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT);
#endif /* !J9VM_INTERP_TWO_PASS_EXCLUSIVE */
				VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT, true);				
				VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT, true);
			} else if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT | J9_PUBLIC_FLAGS_VM_ACCESS)) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
				responsesExpected++;
			}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
			if (J9_ARE_ANY_BITS_SET(currentThread->publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT | J9_PUBLIC_FLAGS_VM_ACCESS)) {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
				responsesExpected++;
			}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			else {
#if defined(J9VM_INTERP_TWO_PASS_EXCLUSIVE)
				VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT);
#endif /* J9VM_INTERP_TWO_PASS_EXCLUSIVE */
				VM_VMAccess::setPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT, true);				
				VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT, true);
			}
			omrthread_monitor_exit(currentThread->publicFlagsMutex);
		} while ((currentThread = currentThread->linkNext) != vmThread);
		omrthread_monitor_exit(vm->vmThreadListMutex);
retry:
		/* Wait for all threads to respond to the safe point request */
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		vm->safePointResponseCount += responsesExpected;
		while (0 != vm->safePointResponseCount) {
			omrthread_monitor_wait(vm->exclusiveAccessMutex);
		}
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
		internalAcquireVMAccessWithMask(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE);
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		if (0 != vm->safePointResponseCount) {
			omrthread_monitor_exit(vm->exclusiveAccessMutex);
			internalReleaseVMAccess(vmThread);
			responsesExpected = 0;
			goto retry;
		}
		vm->safePointState = J9_XACCESS_EXCLUSIVE;
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
		// Not necessary?
		VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
	Assert_VM_true(J9_XACCESS_EXCLUSIVE == vm->safePointState);
	omrthread_monitor_enter(vm->vmThreadListMutex);
}

void
releaseSafePointVMAccess(J9VMThread * vmThread)
{
	J9JavaVM* vm = vmThread->javaVM;

	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_VM_ACCESS) {
		Assert_VM_true(currentVMThread(vm) == vmThread);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
	Assert_VM_false(vmThread->safePointCount == 0);
	Assert_VM_true(J9_XACCESS_EXCLUSIVE == vm->safePointState);

	if (--(vmThread->safePointCount) == 0) {
		J9VMThread *currentThread = vmThread;
		do {
			VM_VMAccess::clearPublicFlags(currentThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT | J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT, true);
		} while ((currentThread = currentThread->linkNext) != vmThread);
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		vm->safePointState = J9_XACCESS_NONE;
		omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
		omrthread_monitor_exit(vm->vmThreadListMutex);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
}

/*
 * The current thread must have vm access when calling this function.
 *
 * Note: While the current thread has another thread halted, it must not do anything to modify
 * it's own stack, including the creation of JNI local refs, pushObjectInSpecialFrame, or the
 * running of any java code.
 */

void
haltThreadForInspection(J9VMThread * currentThread, J9VMThread * vmThread)
{

_tryAgain:

	Assert_VM_mustHaveVMAccess(currentThread);

	/* Inspecting the current thread does not require any halting */
	if (currentThread != vmThread) {
		omrthread_monitor_enter(vmThread->publicFlagsMutex);

		/* increment the inspection count but don't try to short circuit -- the thread might not actually be halted yet */
		vmThread->inspectionSuspendCount += 1;

		/* Now halt the thread for inspection */
		setHaltFlag(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION);

		/* If the thread doesn't have VM access and it not queued for exclusive we can proceed immediately */
		if (vmThread->publicFlags & (J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_QUEUED_FOR_EXCLUSIVE)) {
			/* Release VM access while waiting */
			/* (We must release the other thread's publicFlagsMutex to avoid deadlock here) */
			omrthread_monitor_exit(vmThread->publicFlagsMutex);
			internalReleaseVMAccess(currentThread);
			omrthread_monitor_enter(vmThread->publicFlagsMutex);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
			flushProcessWriteBuffers(vmThread->javaVM);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
			VM_AtomicSupport::readWriteBarrier(); // necessary?
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			if (VM_VMAccess::mustWaitForVMAccessRelease(vmThread)) {
				while (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_QUEUED_FOR_EXCLUSIVE)) {
					omrthread_monitor_wait(vmThread->publicFlagsMutex);
				}
			}
			omrthread_monitor_exit(vmThread->publicFlagsMutex);

			/* Thread is halted - reacquire VM access */
	
			omrthread_monitor_enter(currentThread->publicFlagsMutex);
			internalAcquireVMAccessNoMutexWithMask(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_ANY - J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION);
			omrthread_monitor_exit(currentThread->publicFlagsMutex);

			/* If currentThread is being halted, cancel vmThread's pending inspection request */
			if (J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION == (currentThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION)) {
				resumeThreadForInspection(currentThread, vmThread);
				goto _tryAgain;
			}

		} else {
			/* the thread doesn't have VM access so we don't need to wait for it */
			omrthread_monitor_exit(vmThread->publicFlagsMutex);
		}
	}

	Assert_VM_mustHaveVMAccess(currentThread);
}

/* Note that VM access is released and reacquired by this call - direct object pointers must not be held across this call */

void
resumeThreadForInspection(J9VMThread * currentThread, J9VMThread * vmThread)
{
	/* Inspecting the current thread does not require any halting */

	if (currentThread != vmThread) {
		/* Ignore resumes for threads which have not been suspended for inspection */

		omrthread_monitor_enter(vmThread->publicFlagsMutex);
		if (vmThread->inspectionSuspendCount != 0) {
			if (--vmThread->inspectionSuspendCount == 0) {
				clearHaltFlag(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION);
			}
		}
		omrthread_monitor_exit(vmThread->publicFlagsMutex);

		/* was the current thread running with partial VM access? */
		/* (It is safe to read the publicFlags without a mutex since we're only really interested if it was set before we acquired VM access) */
		if (currentThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION) {
			/* reacquire full VM access */
			internalReleaseVMAccess(currentThread);
			internalAcquireVMAccess(currentThread);
		}
	}
}

} /* extern "C" */
