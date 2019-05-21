/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
#include "j9protos.h"
#include "ModronAssertions.h"

#include "EnvironmentBase.hpp"
#include "EnvironmentDelegate.hpp"
#include "GCExtensions.hpp"
#include "JNICriticalRegion.hpp"
#include "OMRVMInterface.hpp"
#include "OwnableSynchronizerObjectBufferRealtime.hpp"
#include "OwnableSynchronizerObjectBufferStandard.hpp"
#include "OwnableSynchronizerObjectBufferVLHGC.hpp"
#include "ReferenceObjectBufferRealtime.hpp"
#include "ReferenceObjectBufferStandard.hpp"
#include "ReferenceObjectBufferVLHGC.hpp"
#include "SublistFragment.hpp"
#include "UnfinalizedObjectBufferRealtime.hpp"
#include "UnfinalizedObjectBufferStandard.hpp"
#include "UnfinalizedObjectBufferVLHGC.hpp"
#include "VMAccess.hpp"
#include "omrlinkedlist.h"

/**
 * Initialize the delegate's internal structures and values.
 */
bool
MM_EnvironmentDelegate::initialize(MM_EnvironmentBase *env)
{
	_env = env;

	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	if (extensions->isStandardGC()) {
#if defined(OMR_GC_MODRON_STANDARD)
		_gcEnv._referenceObjectBuffer = MM_ReferenceObjectBufferStandard::newInstance(env);
		_gcEnv._unfinalizedObjectBuffer = MM_UnfinalizedObjectBufferStandard::newInstance(env);
		_gcEnv._ownableSynchronizerObjectBuffer = MM_OwnableSynchronizerObjectBufferStandard::newInstance(env);
#endif /* defined(OMR_GC_MODRON_STANDARD) */
	} else if (extensions->isMetronomeGC()) {
#if defined(OMR_GC_REALTIME)
		_gcEnv._referenceObjectBuffer = MM_ReferenceObjectBufferRealtime::newInstance(env);
		_gcEnv._unfinalizedObjectBuffer = MM_UnfinalizedObjectBufferRealtime::newInstance(env);
		_gcEnv._ownableSynchronizerObjectBuffer = MM_OwnableSynchronizerObjectBufferRealtime::newInstance(env);
#endif /* defined(OMR_GC_REALTIME) */
	} else if (extensions->isVLHGC()) {
#if defined(OMR_GC_VLHGC)
		_gcEnv._referenceObjectBuffer = MM_ReferenceObjectBufferVLHGC::newInstance(env);
		_gcEnv._unfinalizedObjectBuffer = MM_UnfinalizedObjectBufferVLHGC::newInstance(env);
		_gcEnv._ownableSynchronizerObjectBuffer = MM_OwnableSynchronizerObjectBufferVLHGC::newInstance(env);
#endif /* defined(OMR_GC_VLHGC) */
	} else {
		Assert_MM_unreachable();
	}

	if ((NULL == _gcEnv._referenceObjectBuffer) ||
			(NULL == _gcEnv._unfinalizedObjectBuffer) ||
			(NULL == _gcEnv._ownableSynchronizerObjectBuffer)) {
		return false;
	}

	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_EnvironmentDelegate::tearDown()
{
	if (NULL != _gcEnv._referenceObjectBuffer) {
		_gcEnv._referenceObjectBuffer->kill(_env);
		_gcEnv._referenceObjectBuffer = NULL;
	}

	if (NULL != _gcEnv._unfinalizedObjectBuffer) {
		_gcEnv._unfinalizedObjectBuffer->kill(_env);
		_gcEnv._unfinalizedObjectBuffer = NULL;
	}

	if (NULL != _gcEnv._ownableSynchronizerObjectBuffer) {
		_gcEnv._ownableSynchronizerObjectBuffer->kill(_env);
		_gcEnv._ownableSynchronizerObjectBuffer = NULL;
	}
}

OMR_VMThread *
MM_EnvironmentDelegate::attachVMThread(OMR_VM *omrVM, const char  *threadName, uintptr_t reason)
{
	J9JavaVM *javaVM = (J9JavaVM *)omrVM->_language_vm;
	J9VMThread *vmThread = NULL;
	if (JNI_OK != javaVM->internalVMFunctions->attachSystemDaemonThread(javaVM, &vmThread, threadName)) {
		return NULL;
	}

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if ((MM_EnvironmentBase::ATTACH_THREAD != reason) &&  (NULL != javaVM->javaOffloadSwitchOnWithReasonFunc)) {
		(*javaVM->javaOffloadSwitchOnWithReasonFunc)(vmThread, J9_JNI_OFFLOAD_SWITCH_FINALIZE_SLAVE_THREAD + reason);
		vmThread->javaOffloadState = 1;
	}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */

	return vmThread->omrVMThread;
}

void
MM_EnvironmentDelegate::detachVMThread(OMR_VM *omrVM, OMR_VMThread *omrThread, uintptr_t reason)
{
	J9JavaVM *javaVM = (J9JavaVM *)omrVM->_language_vm;

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if ((MM_EnvironmentBase::ATTACH_THREAD != reason) &&  (NULL != javaVM->javaOffloadSwitchOnWithReasonFunc)) {
		(*javaVM->javaOffloadSwitchOffWithReasonFunc)((J9VMThread *)omrThread->_language_vmthread, reason);
	}
#endif

	javaVM->internalVMFunctions->DetachCurrentThread((JavaVM *)javaVM);
}

void
MM_EnvironmentDelegate::flushNonAllocationCaches()
{
#if defined(J9VM_GC_GENERATIONAL)
	if (_env->getExtensions()->isStandardGC()) {
		MM_SublistFragment::flush((J9VMGC_SublistFragment*)&((J9VMThread *)_env->getLanguageVMThread())->gcRememberedSet);
	}
#endif /* J9VM_GC_GENERATIONAL */

#if defined(J9VM_GC_FINALIZATION)
	_gcEnv._unfinalizedObjectBuffer->flush(_env);
#endif /* J9VM_GC_FINALIZATION */

	_gcEnv._ownableSynchronizerObjectBuffer->flush(_env);
}

void
MM_EnvironmentDelegate::setGCMasterThread(bool isMasterThread)
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	if (isMasterThread) {
		vmThread->privateFlags |= J9_PRIVATE_FLAGS_GC_MASTER_THREAD;
	} else {
		vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_GC_MASTER_THREAD;
	}
}

void
MM_EnvironmentDelegate::acquireVMAccess()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	vmThread->javaVM->internalVMFunctions->internalAcquireVMAccess(vmThread);
}

void
MM_EnvironmentDelegate::releaseVMAccess()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	vmThread->javaVM->internalVMFunctions->internalReleaseVMAccess(vmThread);
}

bool
MM_EnvironmentDelegate::isExclusiveAccessRequestWaiting()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	return (J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE == (vmThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_EXCLUSIVE));
}

/**
 * Acquires exclusive VM access
 */
void
MM_EnvironmentDelegate::acquireExclusiveVMAccess()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	vmThread->javaVM->internalVMFunctions->acquireExclusiveVMAccess(vmThread);
}

/**
 * Releases exclusive VM access.
 */
void
MM_EnvironmentDelegate::releaseExclusiveVMAccess()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	vmThread->javaVM->internalVMFunctions->releaseExclusiveVMAccess(vmThread);
}


uintptr_t
MM_EnvironmentDelegate::relinquishExclusiveVMAccess()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	uintptr_t savedExclusiveCount = vmThread->omrVMThread->exclusiveCount;

	Assert_MM_true(J9_PUBLIC_FLAGS_VM_ACCESS == (vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS));
	Assert_MM_true(0 < savedExclusiveCount);

	vmThread->omrVMThread->exclusiveCount = 0;
	VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_VM_ACCESS);

	return savedExclusiveCount;
}

void
MM_EnvironmentDelegate::assumeExclusiveVMAccess(uintptr_t exclusiveCount)
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;

	Assert_MM_true(exclusiveCount >= 1);
	Assert_MM_true(0 == (vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS));
	Assert_MM_true(0 == vmThread->omrVMThread->exclusiveCount);

	vmThread->omrVMThread->exclusiveCount = exclusiveCount;
	VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_VM_ACCESS);
}

void
MM_EnvironmentDelegate::releaseCriticalHeapAccess(uintptr_t *data)
{
        J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
        VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);
        MM_JNICriticalRegion::releaseAccess(vmThread, data);
}

void
MM_EnvironmentDelegate::reacquireCriticalHeapAccess(uintptr_t data)
{
        J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
        MM_JNICriticalRegion::reacquireAccess(vmThread, data);
        VM_VMAccess::clearPublicFlags(vmThread, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT);
}

void
MM_EnvironmentDelegate::forceOutOfLineVMAccess()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getLanguageVMThread();
	VM_VMAccess::setPublicFlags(vmThread, J9_PUBLIC_FLAGS_DISABLE_INLINE_VM_ACCESS);
}

#if defined (J9VM_GC_THREAD_LOCAL_HEAP)
/**
 * Disable inline TLH allocates by hiding the real heap allocation address from
 * JIT/Interpreter in realHeapAlloc and setting heapALloc == HeapTop so TLH
 * looks full.
 *
 */
void
MM_EnvironmentDelegate::disableInlineTLHAllocate()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	J9ModronThreadLocalHeap *tlh = (J9ModronThreadLocalHeap *)&vmThread->allocateThreadLocalHeap;
	tlh->realHeapAlloc = vmThread->heapAlloc;
	vmThread->heapAlloc = vmThread->heapTop;

#if defined(J9VM_GC_NON_ZERO_TLH)
	tlh = (J9ModronThreadLocalHeap *)&vmThread->nonZeroAllocateThreadLocalHeap;
	tlh->realHeapAlloc = vmThread->nonZeroHeapAlloc;
	vmThread->nonZeroHeapAlloc = vmThread->nonZeroHeapTop;
#endif /* defined(J9VM_GC_NON_ZERO_TLH) */
}

/**
 * Re-enable inline TLH allocate by restoring heapAlloc from realHeapAlloc
 */
void
MM_EnvironmentDelegate::enableInlineTLHAllocate()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	J9ModronThreadLocalHeap *tlh = (J9ModronThreadLocalHeap *)&vmThread->allocateThreadLocalHeap;
	vmThread->heapAlloc =  tlh->realHeapAlloc;
	tlh->realHeapAlloc = NULL;

#if defined(J9VM_GC_NON_ZERO_TLH)
	tlh = (J9ModronThreadLocalHeap *)&vmThread->nonZeroAllocateThreadLocalHeap;
	vmThread->nonZeroHeapAlloc =  tlh->realHeapAlloc;
	tlh->realHeapAlloc = NULL;
#endif /* defined(J9VM_GC_NON_ZERO_TLH) */
}

/**
 * Determine if inline TLH allocate is enabled; its enabled if realheapAlloc is NULL.
 * @return TRUE if inline TLH allocates currently enabled for this thread; FALSE otherwise
 */
bool
MM_EnvironmentDelegate::isInlineTLHAllocateEnabled()
{
	J9VMThread *vmThread = (J9VMThread *)_env->getOmrVMThread()->_language_vmthread;
	J9ModronThreadLocalHeap *tlh = (J9ModronThreadLocalHeap *)&vmThread->allocateThreadLocalHeap;
	bool result = (NULL == tlh->realHeapAlloc);

#if defined(J9VM_GC_NON_ZERO_TLH)
	tlh = (J9ModronThreadLocalHeap *)&vmThread->nonZeroAllocateThreadLocalHeap;
	result = result && (NULL == tlh->realHeapAlloc);
#endif /* defined(J9VM_GC_NON_ZERO_TLH) */

	return result;
}
#endif /* J9VM_GC_THREAD_LOCAL_HEAP */

