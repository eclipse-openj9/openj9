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


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(JNICRITICALREGION_HPP_)
#define JNICRITICALREGION_HPP_

#include "modron.h"
#include "ModronAssertions.h"

#include "AtomicSupport.hpp"
#include "BaseNonVirtual.hpp"
#include "VMAccess.hpp"

class MM_JNICriticalRegion : public MM_BaseNonVirtual
{
	/* data members */
private:
protected:
public:

	/* member function */
private:
protected:
public:
	static void reacquireAccess(J9VMThread* vmThread, UDATA accessMask);
	static void releaseAccess(J9VMThread* vmThread, UDATA* accessMask);

	/**
	 * Enter a JNI critical region (i.e. GetPrimitiveArrayCritical or GetStringCritical).
	 * Once a thread has successfully entered a critical region, it has privileges similar
	 * to holding VM access. No object can move while any thread is in a critical region.
	 *
	 * @param vmThread     the J9VMThread requesting to enter a critical region
	 * @param hasVMAccess  true if caller has acquired VM access, false if not
	 */
	static MMINLINE void
	enterCriticalRegion(J9VMThread* vmThread, bool hasVMAccess)
	{
		if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_DEBUG_VM_ACCESS)) {
			Assert_MM_true(J9_VM_FUNCTION(vmThread, currentVMThread)(vmThread->javaVM) == vmThread);
		}

		/* Handle nested case first to avoid the unnecessary atomic */
		if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_JNI_CRITICAL_REGION)) {
			/* Nested critical region; increment the count */
			vmThread->jniCriticalDirectCount += 1;
	  	} else {
			UDATA const criticalFlags = J9_PUBLIC_FLAGS_JNI_CRITICAL_REGION | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			UDATA const expectedFlags = J9_PUBLIC_FLAGS_VM_ACCESS;
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
			UDATA const expectedFlags = hasVMAccess ? J9_PUBLIC_FLAGS_VM_ACCESS : 0;
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			/* Expected case: swap in JNI access bits */
			if (expectedFlags == VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags, expectedFlags, expectedFlags | criticalFlags)) {
				/* First entry into a critical region */
				vmThread->jniCriticalDirectCount = 1;
			} else {
				omrthread_t const osThread = vmThread->osThread;
				omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
				omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
				if (hasVMAccess) {
					/* Entering the first critical region with VM access; set the critical flags */
					VM_VMAccess::setPublicFlags(vmThread, criticalFlags);
					vmThread->jniCriticalDirectCount = 1;

					/* The current thread has VM access and just acquired JNI critical access.
					 * If an exclusive request is in progress and the current thread has already
					 * been requested to halt, then adjust the JNI response count accordingly.
					 */
					if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE)) {
						J9JavaVM* const vm = vmThread->javaVM;
						omrthread_monitor_t const exclusiveAccessMutex = vm->exclusiveAccessMutex;
						omrthread_monitor_enter_using_threadId(exclusiveAccessMutex, osThread);
						vm->jniCriticalResponseCount += 1;
						omrthread_monitor_exit_using_threadId(exclusiveAccessMutex, osThread);
					}
				} else {
					/* Entering the first critical region; acquire VM access and set the critical flags */
					if (0 == VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags, 0, criticalFlags)) {
						/* Set the count to 1 */
						vmThread->jniCriticalDirectCount = 1;
					} else {
						J9_VM_FUNCTION(vmThread, internalEnterVMFromJNI)(vmThread);
						VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_JNI_CRITICAL_REGION | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS);
						vmThread->jniCriticalDirectCount = 1;
						J9_VM_FUNCTION(vmThread, internalExitVMToJNI)(vmThread);
					}
				}
				omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
			}
	 	}
	}

	/**
	 * Exit a JNI critical region (i.e. ReleasePrimitiveArrayCritical or ReleaseStringCritical).
	 * Once a thread has successfully exited a critical region, objects in the java
	 * heap are allowed to move again.
	 *
	 * @param vmThread     the J9VMThread requesting to exit a critical region
	 * @param hasVMAccess  true if caller has acquired VM access, false if not
	 */
	static MMINLINE void
	exitCriticalRegion(J9VMThread* vmThread, bool hasVMAccess)
	{
		if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_DEBUG_VM_ACCESS)) {
			Assert_MM_true(J9_VM_FUNCTION(vmThread, currentVMThread)(vmThread->javaVM) == vmThread);
		}

		Assert_MM_mustHaveJNICriticalRegion(vmThread);
		if (--vmThread->jniCriticalDirectCount == 0) {
			/* Exiting last critical region, swap out critical flags */
			UDATA const criticalFlags = J9_PUBLIC_FLAGS_JNI_CRITICAL_REGION | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			UDATA const finalFlags = J9_PUBLIC_FLAGS_VM_ACCESS;
#else /* J9VM_INTERP_ATOMIC_FREE_JNI */
			UDATA const finalFlags = hasVMAccess ? J9_PUBLIC_FLAGS_VM_ACCESS : 0;
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			UDATA const jniAccess = criticalFlags | finalFlags;
			if (jniAccess != VM_AtomicSupport::lockCompareExchange(&vmThread->publicFlags, jniAccess, finalFlags)) {
				/* Exiting the last critical region; clear the critical flags.
				 * Cache a copy of the flags first to determine if we must respond to an exclusive access request.
				 */
				omrthread_t const osThread = vmThread->osThread;
				omrthread_monitor_t const publicFlagsMutex = vmThread->publicFlagsMutex;
				omrthread_monitor_enter_using_threadId(publicFlagsMutex, osThread);
				UDATA const publicFlags = VM_VMAccess::clearPublicFlagsNoMutex(vmThread, criticalFlags);
				if (J9_ARE_ALL_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS | J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE)) {
					/* If an exclusive request is pending, then respond. */
					J9JavaVM* const vm = vmThread->javaVM;
					omrthread_monitor_t const exclusiveAccessMutex = vm->exclusiveAccessMutex;
					omrthread_monitor_enter_using_threadId(exclusiveAccessMutex, osThread);
					PORT_ACCESS_FROM_JAVAVM(vm);
					U_64 const timeNow = VM_VMAccess::updateExclusiveVMAccessStats(vmThread, vm, PORTLIB);
					if (--vm->jniCriticalResponseCount == 0) {
						VM_VMAccess::respondToExclusiveRequest(vmThread, vm, PORTLIB, timeNow, J9_EXCLUSIVE_SLOW_REASON_JNICRITICAL);
					}
					omrthread_monitor_exit_using_threadId(exclusiveAccessMutex, osThread);
				}
				omrthread_monitor_exit_using_threadId(publicFlagsMutex, osThread);
			}
		}
	}
};

#endif /* JNICRITICALREGION_HPP_ */
