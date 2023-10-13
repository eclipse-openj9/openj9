/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
#if !defined(CONTINUATIONHELPERS_HPP_)
#define CONTINUATIONHELPERS_HPP_

#include "j9.h"
#include "j9consts.h"
#include "j9vmconstantpool.h"

/* These should match the error code values in enum Pinned within class Continuation. */
#define J9VM_CONTINUATION_PINNED_REASON_NATIVE 1
#define J9VM_CONTINUATION_PINNED_REASON_MONITOR 2
#define J9VM_CONTINUATION_PINNED_REASON_CRITICAL_SECTION 3

class VM_ContinuationHelpers {
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

	static VMINLINE void
	swapFieldsWithContinuation(J9VMThread *vmThread, J9VMContinuation *continuation, j9object_t continuationObject, bool swapJ9VMthreadSavedRegisters = true)
	{
	/* Helper macro to swap fields between the two J9Class structs. */
#define SWAP_MEMBER(fieldName, fieldType, class1, class2) \
	do { \
		fieldType temp = (fieldType)((class1)->fieldName); \
		(class1)->fieldName = (class2)->fieldName; \
		(class2)->fieldName = (fieldType)temp; \
	} while (0)

		SWAP_MEMBER(arg0EA, UDATA*, vmThread, continuation);
		SWAP_MEMBER(bytecodes, UDATA*, vmThread, continuation);
		SWAP_MEMBER(sp, UDATA*, vmThread, continuation);
		SWAP_MEMBER(pc, U_8*, vmThread, continuation);
		SWAP_MEMBER(literals, J9Method*, vmThread, continuation);
		SWAP_MEMBER(stackOverflowMark, UDATA*, vmThread, continuation);
		SWAP_MEMBER(stackOverflowMark2, UDATA*, vmThread, continuation);
		SWAP_MEMBER(stackObject, J9JavaStack*, vmThread, continuation);
		SWAP_MEMBER(decompilationStack, J9JITDecompilationInfo*, vmThread, continuation);
		SWAP_MEMBER(j2iFrame, UDATA*, vmThread, continuation);
		SWAP_MEMBER(dropFlags, UDATA, vmThread, continuation);

		J9VMEntryLocalStorage *threadELS = vmThread->entryLocalStorage;
		/* Swap the JIT GPR registers data referenced by ELS */
		J9JITGPRSpillArea tempGPRs = continuation->jitGPRs;
		J9I2JState tempI2J = continuation->i2jState;
		continuation->jitGPRs = *(J9JITGPRSpillArea*)threadELS->jitGlobalStorageBase;
		continuation->i2jState = threadELS->i2jState;
		if (swapJ9VMthreadSavedRegisters) {
			*(J9JITGPRSpillArea*)threadELS->jitGlobalStorageBase = tempGPRs;
		}
		threadELS->i2jState = tempI2J;
		SWAP_MEMBER(oldEntryLocalStorage, J9VMEntryLocalStorage*, threadELS, continuation);

		j9object_t scopedValueCache = J9VMJDKINTERNALVMCONTINUATION_SCOPEDVALUECACHE(vmThread, continuationObject);
		J9VMJDKINTERNALVMCONTINUATION_SET_SCOPEDVALUECACHE(vmThread, continuationObject, vmThread->scopedValueCache);
		vmThread->scopedValueCache = scopedValueCache;
	}

	static VMINLINE ContinuationState volatile *
	getContinuationStateAddress(J9VMThread *vmThread , j9object_t object)
	{
		return (ContinuationState volatile *) ((uintptr_t) object + J9VMJDKINTERNALVMCONTINUATION_STATE_OFFSET(vmThread));
	}

	static VMINLINE bool
	isStarted(ContinuationState continuationState)
	{
		return J9_ARE_ALL_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_STARTED);
	}

	static VMINLINE void
	setStarted(ContinuationState volatile *continuationStatePtr)
	{
		*continuationStatePtr |= J9_GC_CONTINUATION_STATE_STARTED;
	}

	static VMINLINE bool
	isLastUnmount(ContinuationState continuationState)
	{
		return J9_ARE_ALL_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_LAST_UNMOUNT);
	}

	static VMINLINE void
	setLastUnmount(ContinuationState volatile *continuationStatePtr)
	{
		*continuationStatePtr |= J9_GC_CONTINUATION_STATE_LAST_UNMOUNT;
	}

	static VMINLINE bool
	isFinished(ContinuationState continuationState)
	{
		return J9_ARE_ALL_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_FINISHED);
	}

	static VMINLINE void
	setFinished(ContinuationState volatile *continuationStatePtr)
	{
		*continuationStatePtr |= J9_GC_CONTINUATION_STATE_FINISHED;
	}

	static VMINLINE bool
	isActive(ContinuationState continuationState)
	{
		return isStarted(continuationState) && !isFinished(continuationState);
	}

	static VMINLINE uintptr_t
	getConcurrentGCMask(bool isGlobalGC)
	{
		if (isGlobalGC) {
			return J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_GLOBAL;
		} else {
			return J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_LOCAL;
		}
	}

	/**
	 * Check if the related J9VMContinuation is concurrently scaned from the state
	 * 2 variants with and without bool isGlobalGC param.
	 * without isGlobalGC param, return true if it is either local concurrent scanning case or global concurrent scanning case.
	 * with isGlobalGC param, if isGlobalGC == true, only check if global concurrent scanning case.
	 * 						  if isGlobalGC == false, only check if local concurrent scanning case.
	 */
	static VMINLINE bool
	isConcurrentlyScanned(ContinuationState continuationState)
	{
		return J9_ARE_ANY_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_CONCURRENT_SCAN_ANY);
	}

	static VMINLINE bool
	isConcurrentlyScanned(ContinuationState continuationState, bool isGlobalGC)
	{
		uintptr_t concurrentGCMask = getConcurrentGCMask(isGlobalGC);
		return J9_ARE_ALL_BITS_SET(continuationState, concurrentGCMask);
	}

	static VMINLINE void
	setConcurrentlyScanned(ContinuationState *continuationStatePtr, bool isGlobalGC)
	{
		*continuationStatePtr |= getConcurrentGCMask(isGlobalGC);
	}

	static VMINLINE void
	resetConcurrentlyScanned(ContinuationState *continuationStatePtr, bool isGlobalGC)
	{
		*continuationStatePtr &= ~getConcurrentGCMask(isGlobalGC);
	}

	static VMINLINE bool
	isPendingToBeMounted(ContinuationState continuationState)
	{
		return J9_ARE_ALL_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_PENDING_TO_BE_MOUNTED);
	}

	static VMINLINE void
	resetPendingState(ContinuationState *continuationStatePtr)
	{
		*continuationStatePtr &= ~J9_GC_CONTINUATION_STATE_PENDING_TO_BE_MOUNTED;
	}

	/**
	 * Check if the related continuation is fully mounted to carrier thread
	 * if carrierThreadID has been set in ContinuationState, the continuation might be mounted,
	 * there also is pending to be mounted case, when the mounting is blocked by the concurrent continuation
	 * scanning or related vm access.
	 *
	 * @param[in] continuationState the related continuationObject->state
	 * @return true if it is mounted.
	 */
	static VMINLINE bool
	isFullyMounted(ContinuationState continuationState)
	{
		bool mounted = J9_ARE_ANY_BITS_SET(continuationState, J9_GC_CONTINUATION_STATE_CARRIERID_MASK);
		if (mounted && isPendingToBeMounted(continuationState)) {
			mounted = false;
		}
		return mounted;
	}

	static VMINLINE J9VMThread *
	getCarrierThread(ContinuationState continuationState)
	{
		return (J9VMThread *)(continuationState & J9_GC_CONTINUATION_STATE_CARRIERID_MASK);
	}

	static VMINLINE bool
	isMountedWithCarrierThread(ContinuationState continuationState, J9VMThread *carrierThread)
	{
		return carrierThread == getCarrierThread(continuationState);
	}

	static VMINLINE void
	settingCarrierAndPendingState(ContinuationState *continuationStatePtr, J9VMThread *carrierThread)
	{
		/* also set PendingToBeMounted */
		*continuationStatePtr |= (uintptr_t)carrierThread | J9_GC_CONTINUATION_STATE_PENDING_TO_BE_MOUNTED;
	}

	static VMINLINE void
	resetCarrierID(ContinuationState volatile *continuationStatePtr)
	{
		*continuationStatePtr &= ~J9_GC_CONTINUATION_STATE_CARRIERID_MASK;
	}

	/**
	 * Return the thread object whose state is stored in the J9VMContinuation.
	 *
	 * @param[in] vmThread the current thread
	 * @param[in] continuation the native continuation structure
	 * @param[in] continuationObject the continuation object
	 * @return the thread object whose state is stored in the continuation
	 */
	static VMINLINE j9object_t
	getThreadObjectForContinuation(J9VMThread *vmThread, J9VMContinuation *continuation, j9object_t continuationObject)
	{
		/* threadObject points to the virtual thread. */
		j9object_t threadObject = (j9object_t)J9VMJDKINTERNALVMCONTINUATION_VTHREAD(vmThread, continuationObject);
		ContinuationState volatile *continuationStatePtr = getContinuationStateAddress(vmThread, continuationObject);
		ContinuationState continuationState = *continuationStatePtr;

		if (isFullyMounted(continuationState)) {
			/* If the continuation is fully mounted, then the continuation stores the state of the carrier thread.
			 * Below, threadObject points to the carrier thread.
			 */
			threadObject = (j9object_t)J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(vmThread, threadObject);
		}

		return threadObject;
	}
};

#endif /* CONTINUATIONHELPERS_HPP_ */
