/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(VMACCESS_HPP_)
#define VMACCESS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9comp.h"

#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

#define J9_EXCLUSIVE_SLOW_TOLERANCE_REALTIME 5
#define J9_EXCLUSIVE_SLOW_TOLERANCE_STANDARD 50
#define J9_EXCLUSIVE_SLOW_REASON_JNICRITICAL 1
#define J9_EXCLUSIVE_SLOW_REASON_EXCLUSIVE   2

class VM_VMAccess
{
/*
 * Data members
 */
private:

protected:

public:

/*
 * Function members
 */
private:

protected:

public:
	static VMINLINE bool
	inlineOnlyTryAcquireVMAccess(J9VMThread *const vmThread)
	{
		bool acquired = false;
		if (0 == VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags, 0, J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS)) {
			VM_AtomicSupport::readBarrier();
			acquired = true;
		}
		return acquired;
	}

	static VMINLINE bool
	inlineTryAcquireVMAccess(J9VMThread *const vmThread)
	{
		bool acquired = inlineOnlyTryAcquireVMAccess(vmThread);
		if (!acquired) {
			/* out-of-line try-acquire */
			omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
			omrthread_t const osThread = vmThread->osThread;
			omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
			if (0 == internalTryAcquireVMAccessNoMutex(vmThread)) {
				acquired = true;
			}
			omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
		}
		return acquired;
	}

	static VMINLINE void
	inlineAcquireVMAccess(J9VMThread *const vmThread)
	{
		if (!inlineOnlyTryAcquireVMAccess(vmThread)) {
			/* out-of-line acquire */
			omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
			omrthread_t const osThread = vmThread->osThread;
			omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
			J9_VM_FUNCTION(vmThread, internalAcquireVMAccessNoMutex)(vmThread);
			omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
		}
	}

	static VMINLINE void
	inlineAcquireVMAccessExhaustiveCheck(J9VMThread *const vmThread)
	{
		{
			UDATA savedPublicFlags = vmThread->publicFlags;
			UDATA publicFlags = 0;

			for (;;) {
				if (savedPublicFlags & J9_PUBLIC_FLAGS_VMACCESS_OUTOFLINE_MASK) {
					break;
				}

				publicFlags = VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags,
						savedPublicFlags, savedPublicFlags | J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS);
				if (savedPublicFlags == publicFlags) {
					/* success */
					VM_AtomicSupport::readBarrier();
					return;
				}

				/* update the saved value and try again */
				savedPublicFlags = publicFlags;
			}
		}

		omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
		omrthread_t const osThread = vmThread->osThread;
		omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
		J9_VM_FUNCTION(vmThread, internalAcquireVMAccessNoMutex)(vmThread);
		omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
	}

	static VMINLINE void
	inlineAcquireVMAccessClearStatus(J9VMThread *const vmThread, const UDATA flags)
	{
		{
			UDATA savedPublicFlags = vmThread->publicFlags;
			UDATA publicFlags = 0;

			for (;;) {
				if (savedPublicFlags & J9_PUBLIC_FLAGS_VMACCESS_OUTOFLINE_MASK) {
					break;
				}

				publicFlags = VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags,
						savedPublicFlags, (savedPublicFlags | J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS) & ~flags);
				if (savedPublicFlags == publicFlags) {
					/* success */
					VM_AtomicSupport::readBarrier();
					return;
				}

				/* update the saved value and try again */
				savedPublicFlags = publicFlags;
			}
		}

		omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
		omrthread_t const osThread = vmThread->osThread;
		omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
		VM_VMAccess::clearPublicFlags(vmThread, flags);
		J9_VM_FUNCTION(vmThread, internalAcquireVMAccessNoMutex)(vmThread);
		omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
	}

	static VMINLINE void
	inlineReleaseVMAccess(J9VMThread *const vmThread)
	{
		UDATA savedPublicFlags = vmThread->publicFlags;
		UDATA publicFlags = 0;

		VM_AtomicSupport::writeBarrier();
		for (;;) {
			if (savedPublicFlags & J9_PUBLIC_FLAGS_VMACCESS_RELEASE_BITS) {
				break;
			}

			publicFlags = VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags,
					savedPublicFlags, savedPublicFlags & (~J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS));
			if (savedPublicFlags == publicFlags) {
				/* success */
				return;
			}

			/* update the saved value and try again */
			savedPublicFlags = publicFlags;
		}

		omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
		omrthread_t const osThread = vmThread->osThread;
		omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
		J9_VM_FUNCTION(vmThread, internalReleaseVMAccessNoMutex)(vmThread);
		omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
	}

	static VMINLINE void
	inlineReleaseVMAccessSetStatus(J9VMThread *const vmThread, const UDATA flags)
	{
		UDATA savedPublicFlags = vmThread->publicFlags;
		UDATA publicFlags = 0;

		VM_AtomicSupport::writeBarrier();
		for (;;) {
			if (savedPublicFlags & J9_PUBLIC_FLAGS_VMACCESS_RELEASE_BITS) {
				break;
			}

			publicFlags = VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags,
					savedPublicFlags, (savedPublicFlags & (~J9_PUBLIC_FLAGS_VMACCESS_ACQUIRE_BITS)) | flags);
			if (savedPublicFlags == publicFlags) {
				/* success */
				return;
			}

			/* update the saved value and try again */
			savedPublicFlags = publicFlags;
		}

		omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
		omrthread_t const osThread = vmThread->osThread;
		omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
		VM_VMAccess::setPublicFlags(vmThread, flags);
		J9_VM_FUNCTION(vmThread, internalReleaseVMAccessNoMutex)(vmThread);
		omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
	}

	/**
	 * Atomically OR flags into the publicFlags of a J9VMThread.
	 * Optionally indicate that an async event is pending.
	 *
	 * @param vmThread[in] the J9VMThread to modify
	 * @param flags[in] the flags to OR in
	 * @param indicateEvent[in] true to indicate an async pending, false (the default) not to
	 */
	static VMINLINE void
	setPublicFlags(J9VMThread *vmThread, UDATA flags, bool indicateEvent = false)
	{
		VM_AtomicSupport::bitOr(&vmThread->publicFlags, flags);
		if (indicateEvent) {
			VM_VMHelpers::indicateAsyncMessagePending(vmThread);
		}
	}

	/**
	 * Atomically AND flags out of the publicFlags of a J9VMThread.
	 * Optionally notify the publicFlagsMutex of the thread.
	 *
	 * @pre The current thread owns the publicFlagsMutex of vmThread if notifyThread is true.
	 *
	 * @param vmThread[in] the J9VMThread to modify
	 * @param flags[in] the flags to AND out
	 * @param notifyThread[in] true to indicate notify the mutex, false (the default) not to
	 *
	 * @return the publicFlags value before the bits were cleared
	 */
	static VMINLINE UDATA
	clearPublicFlagsNoMutex(J9VMThread *vmThread, UDATA flag, bool notifyThread = false)
	{
		UDATA const oldFlags = VM_AtomicSupport::bitAnd(&vmThread->publicFlags, ~flag);
		if (notifyThread) {
			omrthread_monitor_notify_all(vmThread->publicFlagsMutex);
		}
		return oldFlags;
	}

	/**
	 * Atomically AND flags out of the publicFlags of a J9VMThread.
	 * Optionally notify the publicFlagsMutex of the thread.
	 *
	 * @param vmThread[in] the J9VMThread to modify
	 * @param flags[in] the flags to AND out
	 * @param notifyThread[in] true to notify the mutex, false (the default) not to
	 */
	static VMINLINE void
	clearPublicFlags(J9VMThread *vmThread, UDATA flags, bool notifyThread = false)
	{
		if (notifyThread) {
			omrthread_monitor_enter(vmThread->publicFlagsMutex);
		}
		clearPublicFlagsNoMutex(vmThread, flags, notifyThread);
		if (notifyThread) {
			omrthread_monitor_exit(vmThread->publicFlagsMutex);
		}
	}

	/**
	 * Update the vm's J9ExclusiveVMStats structure once currentThread has responded.
	 *
	 * @parm[in] currentThread the thread responding
	 * @parm[in] vm the J9JavaVM
	 * @parm[in] portLibrary the port library
	 *
	 * @return the current time used for the statistics
	 */
	static VMINLINE U_64
	updateExclusiveVMAccessStats(J9VMThread* currentThread, J9JavaVM *vm, J9PortLibrary *portLibrary)
	{
		PORT_ACCESS_FROM_PORT(portLibrary);
		/* update stats */
		U_64 const exclusiveStartTime = vm->omrVM->exclusiveVMAccessStats.startTime;
		U_64 timeNow = j9time_hires_clock();
		if (timeNow < exclusiveStartTime) {
			/* don't let time go backwards */
			timeNow = exclusiveStartTime;
		}
		vm->omrVM->exclusiveVMAccessStats.totalResponseTime += (timeNow - exclusiveStartTime);
		vm->omrVM->exclusiveVMAccessStats.lastResponder = (NULL == currentThread ? NULL : currentThread->omrVMThread);
		vm->omrVM->exclusiveVMAccessStats.haltedThreads += 1;
		return timeNow;
	}

	/**
	 * Respond to an exclusive access request.  Caller must hold vm->exclusiveAccessMutex, which
	 * will be notified (and not released) by this function.
	 *
	 * @parm[in] currentThread the thread responding
	 * @parm[in] vm the J9JavaVM
	 * @parm[in] portLibrary the port library
	 * @parm[in] timeNow the value returned from updateExclusiveVMAccessStats
	 */
	static VMINLINE void
	respondToExclusiveRequest(J9VMThread *currentThread, J9JavaVM *vm, J9PortLibrary *portLibrary, U_64 timeNow, UDATA reason)
	{
		PORT_ACCESS_FROM_PORT(portLibrary);
		U_64 const timeTaken = j9time_hires_delta(vm->omrVM->exclusiveVMAccessStats.startTime, timeNow, J9PORT_TIME_DELTA_IN_MILLISECONDS);
		UDATA slowTolerance = J9_EXCLUSIVE_SLOW_TOLERANCE_STANDARD;
		if (OMR_GC_ALLOCATION_TYPE_SEGREGATED == vm->gcAllocationType) {
			slowTolerance = J9_EXCLUSIVE_SLOW_TOLERANCE_REALTIME;
		}
		if (timeTaken > slowTolerance) {
			TRIGGER_J9HOOK_VM_SLOW_EXCLUSIVE(vm->hookInterface, currentThread, (UDATA) timeTaken, reason);
		}
		omrthread_monitor_notify_all(vm->exclusiveAccessMutex);
	}

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)

	static VMINLINE void
	inlineEnterVMFromJNI(J9VMThread* const currentThread)
	{
		currentThread->inNative = FALSE;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
		VM_AtomicSupport::compilerReorderingBarrier();
#else /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
		VM_AtomicSupport::readWriteBarrier(); // necessary?
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
		if (J9_UNEXPECTED(currentThread->publicFlags != J9_PUBLIC_FLAGS_VM_ACCESS))	{
			J9_VM_FUNCTION(currentThread, internalEnterVMFromJNI)(currentThread);
		}
	}

	static VMINLINE void
	inlineExitVMToJNI(J9VMThread* const currentThread)
	{
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
		VM_AtomicSupport::compilerReorderingBarrier();
		currentThread->inNative = TRUE;
		VM_AtomicSupport::compilerReorderingBarrier();
#else /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
		VM_AtomicSupport::writeBarrier();
		currentThread->inNative = TRUE;
		VM_AtomicSupport::readWriteBarrier(); // necessary?
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
		if (J9_UNEXPECTED(currentThread->publicFlags != J9_PUBLIC_FLAGS_VM_ACCESS)) {
			J9_VM_FUNCTION(currentThread, internalExitVMToJNI)(currentThread);
		}
	}

#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
#define inlineEnterVMFromJNI inlineAcquireVMAccess
#define inlineExitVMToJNI inlineReleaseVMAccess
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

	// assumes PFM is held
	static VMINLINE void
	backOffFromSafePoint(J9VMThread* const vmThread)
	{
		J9JavaVM* const vm = vmThread->javaVM;
		omrthread_monitor_enter(vm->exclusiveAccessMutex);
		if (J9_XACCESS_PENDING == vm->safePointState) {
			if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT)) {
				clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_HALTED_AT_SAFE_POINT);
				setPublicFlags(vmThread, J9_PUBLIC_FLAGS_REQUEST_SAFE_POINT, true);
				if (J9_ARE_NO_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_SAFE_POINT)) {
					vm->safePointResponseCount += 1;
				}
			}
		}
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
	}

	/**
	 * Determine if the VM must wait for the target thread to give up VM
	 * access before it can be inspected.
	 *
	 * Assumes that the target thread publicFlagMutex is owned, and that a halt
	 * bit has been set in the publicFlags to prevent acquisition of VM access.
	 *
	 * @param vmThread[in] the J9VMThread to query
	 *
	 * @return true if the VM must wait, false if not
	 */
	static VMINLINE bool
	mustWaitForVMAccessRelease(J9VMThread* const vmThread)
	{
		bool mustWait = false;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		if (vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
			VM_AtomicSupport::readBarrier(); // necessary?
			if (!vmThread->inNative) {
				mustWait = true;
			}
		}
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
		if (vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) {
			mustWait = true;
		}
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		return mustWait;
	}

	/**
	 * Set halt flags with the intention of forcing the target thread to give up VM access.
	 * Appropriate barriers are issued based on the kind of VM access rules determined
	 * by compile-time flags.
	 *
	 * @param vmThread[in] the J9VMThread to modify
	 * @param flags[in] the flags to set
	 */
	static VMINLINE void
	setHaltFlagForVMAccessRelease(J9VMThread* const vmThread, UDATA const flags)
	{
		setPublicFlags(vmThread, flags, true);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
		J9_VM_FUNCTION(vmThread, flushProcessWriteBuffers)(vmThread->javaVM);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH */
		VM_AtomicSupport::readWriteBarrier(); // necessary?
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	}

};

#endif /* VMACCESS_HPP_ */
