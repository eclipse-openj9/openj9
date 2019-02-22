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

#include "j9.h"

#include "JNICriticalRegion.hpp"

/**
 * Re-acquire VM and/or JNI critical access as specified by the accessMask.
 * This will block if another thread holds exclusive VM access.
 *
 * This function is intended for use with releaseAccess.
 *
 * @param vmThread  the J9VMThread requesting access
 * @param accessMask  the types of access that were held
 */
void
MM_JNICriticalRegion::reacquireAccess(J9VMThread* vmThread, UDATA accessMask)
{
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before acquiring exclusive
	Assert_MM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_DEBUG_VM_ACCESS)) {
		Assert_MM_true(J9_VM_FUNCTION(vmThread, currentVMThread)(vmThread->javaVM) == vmThread);
	}

	Assert_MM_true(0 != (accessMask & (J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS)));
	Assert_MM_true(0 == (accessMask & ~(J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS)));
	omrthread_monitor_enter(vmThread->publicFlagsMutex);
	Assert_MM_true(0 == (vmThread->publicFlags & (J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS)));
	while (vmThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_ANY) {
		omrthread_monitor_wait(vmThread->publicFlagsMutex);
	}

	if (0 != (accessMask & J9_PUBLIC_FLAGS_VM_ACCESS)) {
		TRIGGER_J9HOOK_VM_ACQUIREVMACCESS(vmThread->javaVM->hookInterface, vmThread);

		/* Now that the hook has been invoked, allow inline VM access */
		if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS)) {
			clearEventFlag(vmThread, J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS);
		}
	}

	VM_VMAccess::setPublicFlags(vmThread, accessMask);
	omrthread_monitor_exit(vmThread->publicFlagsMutex);
}

/**
 * Release VM and/or JNI critical access. Record what was held in accessMask.
 * This will respond to any pending exclusive VM access request in progress.
 *
 * This function is intended for use with reacquireAccess.
 *
 * @param vmThread  the J9VMThread requesting access
 * @param accessMask  the types of access to re-acquire
 */
void
MM_JNICriticalRegion::releaseAccess(J9VMThread* vmThread, UDATA* accessMask)
{
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	// Current thread must have entered the VM before acquiring exclusive
	Assert_MM_false(vmThread->inNative);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
	if (J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_DEBUG_VM_ACCESS)) {
		Assert_MM_true(J9_VM_FUNCTION(vmThread, currentVMThread)(vmThread->javaVM) == vmThread);
	}

	omrthread_monitor_enter(vmThread->publicFlagsMutex);
	Assert_MM_true(0 != (vmThread->publicFlags & (J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS)));
	UDATA currentAccess = vmThread->publicFlags & (J9_PUBLIC_FLAGS_VM_ACCESS | J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS);
	Assert_MM_true(0 != currentAccess);
	VM_VMAccess::clearPublicFlags(vmThread, currentAccess);

	if(0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE)) {
		J9JavaVM* vm = vmThread->javaVM;
		PORT_ACCESS_FROM_JAVAVM(vm);
		UDATA shouldRespond = FALSE;

		omrthread_monitor_enter(vm->exclusiveAccessMutex);

		U_64 timeNow = VM_VMAccess::updateExclusiveVMAccessStats(vmThread, vm, PORTLIB);

		if(0 != (currentAccess & J9_PUBLIC_FLAGS_VM_ACCESS)) {
			if (!J9_ARE_ANY_BITS_SET(vmThread->publicFlags, J9_PUBLIC_FLAGS_NOT_COUNTED_BY_EXCLUSIVE)) {
				--vm->exclusiveAccessResponseCount;
				if(0 == vm->exclusiveAccessResponseCount) {
					shouldRespond = TRUE;
				}
			}
		}
		if(0 != (currentAccess & J9_PUBLIC_FLAGS_JNI_CRITICAL_ACCESS)) {
			--vm->jniCriticalResponseCount;
			if(0 == vm->jniCriticalResponseCount) {
				shouldRespond = TRUE;
			}
		}
		if(shouldRespond) {
			VM_VMAccess::respondToExclusiveRequest(vmThread, vm, PORTLIB, timeNow, J9_EXCLUSIVE_SLOW_REASON_JNICRITICAL);
		}
		omrthread_monitor_exit(vm->exclusiveAccessMutex);
	}

	if (0 != (currentAccess & J9_PUBLIC_FLAGS_VM_ACCESS)) {
		TRIGGER_J9HOOK_VM_RELEASEVMACCESS(vmThread->javaVM->hookInterface, vmThread);

		/* Now that the hook has been invoked, allow inline VM access */
		if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS)) {
			clearEventFlag(vmThread, J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS);
		}
	}

	*accessMask = currentAccess;
	omrthread_monitor_exit(vmThread->publicFlagsMutex);
}
